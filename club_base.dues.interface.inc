<?php

/*
 * club_base_dues_interface
 * Cron interface job - mainly the renewal job
 *  1) Read through all member records
 */
function club_base_dues_interface () {
	// Run through all the chapters
  	$query = new EntityFieldQuery();
  	$query->entityCondition('entity_type', 'node')
            ->entityCondition('bundle', 'chapter');
  	$result = $query->execute();
  	if (isset($result['node'])) {
		// Read each chapter's configuration
    	foreach (array_keys($result['node']) as $key) {
			$node = node_load($key);
			club_base_dues_mass_process ($node);
		}
  	}
}

function add_date($givendate,$day=0,$mth=0,$yr=0) {
	$cd = strtotime($givendate);
    $newdate = date('Y-m-d h:i:s', mktime(date('h',$cd),
    	date('i',$cd), date('s',$cd), date('m',$cd)+$mth,
    	date('d',$cd)+$day, date('Y',$cd)+$yr));
    return $newdate;
}

// Process dues criteria for each user in the chapter
function club_base_dues_mass_process ($chapter_node) {
   global $user;

   // build $status array, used for the user query to get only users with particular status's
   $status=array();
   $config = dues_get_config($chapter_node->nid);    // get config for status' to include, else use ALL if none set
   foreach ($config->field_status['und'] as $ary) {
      foreach ($ary as $key => $value) {
         $status[]=$value;
      }
   }

	// Receivable Application Rule (anniversary date or policy date for renewals)	
	$policyruletid = dues_gettid ('dues_application_rule','Policy');
	
   // Calculate the renewal date
   // If renewal is Anniversary, use the Join Date. If Policy, use the day after the current policy's end dated 
   	if ($config->field_dues_application_date['und'][0]['tid'] == $policyruletid) {
		$date = $config->field_policy_date_range['und'][0]['value2'];					// ending date -- renewal is sent exactly 
		$duedate = date('Y-m-d',strtotime($config->field_policy_date_range['und'][0]['value2']. ' +1 day'));		// Due the day after the current policy date range
		$days = $config->field_first_reminder_days['und'][0]['value'].' days';
		$renewal_date = date('Y-m-d',strtotime(substr($date,0,10) . $days));
		$days = $config->field_second_reminder_days['und'][0]['value'].' days';
		$reminder_date = date('Y-m-d',strtotime(substr($date,0,10) . $days));
		
		// Is "today" the renewal date or the reminder date? If not, return
		$today = date('Y-m-d');
		drupal_set_message ($chapter_node->title.': Renew:'.$renewal_date.', remind:'.$reminder_date.', Today:'.$today);
	}
	
   $time = time();
   $ct=0;
   # $date = [node:field_date];

   // Get all the users in this chater with the status contained in the "Status inclusion" array in the dues policy
   $query = new EntityFieldQuery;
   $query
      ->entityCondition('entity_type', 'user')
      ->propertyCondition('status', 1)
      ->propertyCondition('uid',1,'<>');
   if (!empty($status)) {
      $query->fieldCondition('field_status','tid',$status,'IN');
   }
   $query->fieldCondition('field_chapter','target_id',$chapter_node->nid,'=');       // Current Chapter
   $result = $query->execute();

	$today = date('Y-m-d');

   // Process each user to see if they need a reminder
   if (isset($result['user'])) {
     $users = array_keys($result['user']);
     foreach ($users as $uid) {

		$this_user = user_load($uid);
		$profile2 = profile2_load_by_user ($uid,'membership');
		if (isset($profile2->field_date_joined['und'])) {
			$this_user->field_date_joined['und'][0]['value']  = substr($profile2->field_date_joined['und'][0]['value'],0,10);						// imbed for use in token routine
		} else {
			$this_user->field_date_joined['und'][0]['value'] = '1969-01-01';
		}
		$date = $this_user->field_date_joined['und'][0]['value'];	
		$this_user->anniversary = date('Y-m-d',strtotime($date . ' +1 year'));
		
	   	if ($config->field_dues_application_date['und'][0]['tid'] != $policyruletid) {		// by anniversary date, use the date_joined in profile2
			if (isset($profile2->field_date_joined['und'])) {
				$days = $config->field_first_reminder_days['und'][0]['value'].' days';
				$duedate = $date;
				while (strtotime($duedate) < $time) {
					$duedate = date('Y-m-d',strtotime($duedate . ' +1 year'));
				}
				$renewal_date = date('Y-m-d',strtotime(substr($date,0,10) . $days));
				$days = $config->field_first_reminder_days['und'][0]['value'].' days';
				$renewal_date = date('Y-m-d',strtotime(substr($date,0,10) . $days));
				$days = $config->field_second_reminder_days['und'][0]['value'].' days';
				$reminder_date = date('Y-m-d',strtotime(substr($date,0,10) . $days));

if ($this_user->name == 'Able Able') {
	$renewal_date = $today;
	drupal_set_message('Renewal for ' . $this_user->name . ' set for ' . $today);
}
				// Is "today" the renewal date or the reminder date? If not, return
				$today = date('Y-m-d');
			} else {
				drupal_set_message($chapter_node->title . ' no profile2 for ' . $this_user->name);
				continue;
			}
		}
		$this_user->duedate = $duedate;

	// Now, "continue" if today is neither the renewal nor reminder date.
		if ($today != $renewal_date && $today != $reminder_date) {
			continue;
		}
		
		// Now, find out whether or not there's a receivable balance. If zero, there's no balance, so no need to remind
		// Also, prevent duplicate if this is the renewal date
		$balance = club_base_dues_check_dues ($this_user);
		if (($today == $renewal_date && $balance !=0) || ($today==$reminder_date && $balance == 0)) {
			drupal_set_message('Renewal for ' . $this_user->name . ' not done, balance != 0 '. $balance);
			continue;
		}
		drupal_set_message('Renewal for ' . $this_user->name . ' Balance:' . $balance);
		
		// construct and send the email now. Mail can be sent to the member, the dues except the member, or club officers
		// renewal = "Automatic"
		//  	Renewal Date = today .. send first message
		//		Reminder Date = today .. send second message
        
		// setup the addresses -- the member, or if testing, it goes to the test address(es)
			$to = $this_user->mail;
			$test = $config->field_test['und'][0]['value'];
			$testemail = $config->field_test_email_address['und'][0]['email'];

			$from = $config->field_dues_admin_email['und'][0]['email'];						// first admin
			$i = 1;
			$bcc = '';
			$cc = '';
			while (isset($config->field_dues_admin_email['und'][$i]['email'])) {
				if ($i > 1) {
					$bcc = $bcc . ',';		// comma separated
				}
				$bcc = $bcc . $config->field_dues_admin_email['und'][$i]['email'];
				$i++;
			}

		// Now, trigger the appropriate email subject and message
		if ($today == $renewal_date) {

			// set up the subject
			$subject = club_base_token_($config->field_email_subject['und'][0]['value'],$this_user, $config);
			$body = club_base_token_($config->field_email_message['und'][0]['value'],$this_user, $config);
			if ($test == 'Yes') {
				$body = "TEST TO: $to\n\n" . $body;
				$to = $testemail;
			}
			
			//	dues_email ($to, $from, $bcc, $cc, $subject, $body);
				drupal_set_message ('Automatic Renewal');

	          	club_base_dues_email ($to, $from, $bcc, $cc, $subject, $body);
				if ($test == 'Yes') {
					drupal_set_message('Renewal email sent to ' . $to . ' (test mode for '.$this_user->mail.')');
				} else {
					drupal_set_message('Renewal email sent to '. $this_user->mail);
				}
				club_base_dues_create_dues($this_user,$config);

			//	club_base_dues_log();			// log this email for this user
		
		} else if ($today == $reminder_date) {		// reminders go out if the receivable record is unpaid at this time

			// set up the subject
			$subject = club_base_token_($config->field_email_subject2['und'][0]['value'],$this_user, $config);
			$body = club_base_token_($config->field_email_message2['und'][0]['value'],$this_user, $config);
			if ($test == 'Yes') {
				$body = "TEST TO: $to\n\n" . $body;
				$to = $testemail;
			}

			//	dues_email ($to, $from, $bcc, $cc, $subject, $body);
			drupal_set_message ('Reminder');

          	club_base_dues_email ($to, $from, $bcc, $cc, $subject, $body);
			if ($test == 'Yes') {
				drupal_set_message('Renewal email sent to ' . $to . ' (test mode for '.$this_user->mail.')');
			} else {
				drupal_set_message('Renewal email sent to '. $this_user->mail);
			}
	
		//	dues_email ($to, $from, $bcc, $cc, $subject, $body);
			drupal_set_message ('Reminder');
			//	club_base_dues_log();			// log this email for this user
		}
		
		$ct++;
     }
   }
   $msg = 'Membership emails sent for '.$ct.' users';
  watchdog('Action Trigger', t($msg));
}

// check the dues record to see if it's there and paid or partially paid
function club_base_dues_check_dues ($this_user) {
	$query = new EntityFieldQuery;
   	$result = $query
      ->entityCondition('entity_type', 'node')
	  ->entityCondition('bundle','dues')
      ->propertyCondition('status', 1)
      ->fieldCondition('field_uid','target_id',$this_user->uid,'=')
      ->fieldCondition('field_amount_paid','value','0.00','=')
	  ->count()
	  ->execute();
   return $result;			// 0 = no outstanding record
}

// Create the dues record
function club_base_dues_create_dues ($this_user,$config) {
	global $user,$time;
    $node = new stdClass();
    $node->title = 'Dues for ' . $this_user->name;
    $node->nid = '';                  	// add or update, depending on query for 'nid' above^M
	$node->body = '';					// body is hidden
    $node->type = 'dues'; 	// Your specified content type^M
    $node->created = $time;
    $node->changed = $time;
    $node->language = 'und';
    $node->status = 1;
    $node->promote = 0;
    $node->sticky = 0;
    $node->format = 1;       // Filtered HTML^M
    $node->uid = $user->uid;
    $node->field_uid['und'][0]['target_id'] = $this_user->uid;		// user's UID
    $node->field_chapter = $this_user->field_chapter; 				// same chapter as this user
    $node->field_date['und'][0]['value'] = $this_user->duedate .' 00:00:00';							// today
    $node->field_amount_due = $config->field_dues;
    $node->field_amount_paid['und'][0]['value'] = '0.00';
    $node = node_submit($node);
    node_save($node);
	
}

function club_base_dues_log () {
	global $user,$time;
    $node = new stdClass();
    $node->title = 'Notification Log Entry';
    $node->nid = '';                  // add or update, depending on query for 'nid' above^M
    $node->type = 'notification_log';   // Your specified content type^M
    $node->created = $time;
    $node->changed = $time;
    $node->language = 'und';
    $node->status = 1;
    $node->promote = 0;
    $node->sticky = 0;
    $node->format = 1;       // Filtered HTML^M
    $node->uid = $user->uid;
    $node->field_uid['und'][0]['target_id'] = $uid;
    $node->field_chapter = $node->field_chapter; 
    $node->field_date = $node->field_date;
    $node->field_attendance = $node->field_attendance;
    if (isset($node->field_description['und'][0]['value'])) {
      $node->field_description = $node->field_description;
    } else {
      $node->field_description['und'][0]['value'] = '';
    }
    $node = node_submit($node);
    node_save($node);
	
}

/**-----------------------------------------------------------------------------------
 * club_base_token_ : substitute tokens in messages (str_ireplace is case insensitive)
 *  [Dues] [Dues Period] [First Name] [Last Name] [Member] [Join Date] [Anniversary Date]. 
 */
function club_base_token_ ($msg, $this_user,$this_config) {
	$msg = str_ireplace('[dues]',$this_config->field_dues['und'][0]['value'],$msg);
	$msg = str_ireplace('[first name]',$this_user->field_first_name['und'][0]['value'],$msg);
	$msg = str_ireplace('[last name]',$this_user->field_last_name['und'][0]['value'],$msg);
	$msg = str_ireplace('[dues period]',$this_config->field_dues_period['und'][0]['value'],$msg);
	$msg = str_ireplace('[join date]',$this_user->field_date_joined['und'][0]['value'],$msg);		// inserted
	$msg = str_ireplace('[anniversary date]',$this_user->anniversary,$msg);							// inserted
	return $msg;
}

/**----------------------------------------------------------------------------------- 
 * A very simple hook_mail() implementation, for module "mymodule".
 * This implementation expects to receive a $body, $subject, and $headers 
 * inside the $params sent from drupal_mail(), rather than construct
 * those here in the hook.
 * @see drupal_mail()
 */
function club_base_mail($key, &$message, $params) {
  if (isset($params['subject'])) {
    $message['subject'] = $params['subject'];
  } 
  if (isset($params['body'])) {
    $message['body'][] = $params['body']; 
  }
  if (isset($params['headers']) && is_array($params['headers'])) {   
    $message['headers'] += $params['headers'];
  }
  drupal_set_message('hook mail processed');
  // You should really have hook_mail() doing most of the formatting, 
  // rather than pass in a complete $body or a $subject from drupal_mail().  
  // To accomplish that, you'd branch this formatting based on the value of
  // $key sent with drupal_mail(), and using code like this:
  // switch ($key) {
  //  case 'key1':
  //    // do something specific for mails of type key1
  //    break;      
  //  case 'key2':
  //    // do something specific for mails of type key2
  //    break;
  // }
  
  // That's it - You don't do anything in your hook_mail() to actually
  // send the mail, that is taken care of by the mail system automatically. 
}

/****************************************************************************************
 *  attendance_email
 *  Email messages and reports
 */

function club_base_dues_email ($to, $from, $bcc, $cc, $subject, $body) {           // $msg is the message to be emailed - it could be to the member, to an admin, or a group of emails
global $user;  // place at the top of the function you are calling from

        $body = str_replace('&#039;',"'",$body);
        $body = str_replace('<p>',"",$body);
        $body = str_replace('<P>',"",$body);
        $body = str_replace('<br>',"",$body);
        $body = str_replace('<BR>',"",$body);
        $body = str_replace('</p>',"",$body);
        $body = str_replace('</P>',"",$body);
        $body = str_replace('</br>',"\n",$body);
        $body = str_replace('</BR>',"\n",$body);

  	// If you are using suhosin (the hardened PHP extension) and want to use
 	// Cc or Bcc, then make sure that suhosin.mail.protect is not set to 2, 
  	// or the mail will be rejected as an attack.
  	$header_bcc = $bcc;
  	$header_cc = $cc;

  	// you can set 'message_key' to any key you want, and process 
  	// that in the hook_mail as shown in the commented out code.
  	// Or, just set it to some string and ignore it as we are here.
  	// You can add extra headers into the headers array here.  If you
  	// are trying to send an HTML formatted email, please use either
  	// the MIME Mail or HTMLMail modules, rather than doing it
	// by just setting the Content-Type.

  	$message = drupal_mail('dues', 'message', $to, user_preferred_language($user), array('body' => $body, 'subject' => $subject, 'headers' => array('Cc' => $header_cc, 'Bcc' => $header_bcc)), $from, TRUE);
	drupal_set_message('to:' . $to . ' from:'.$from.' Subject:' . $subject . ' body:' . $body);
  	// You can check $message['result'] to see if the message sent successfully.
}

