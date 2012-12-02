<?PHP


/**----------------------------------------------------------------------------------
 *  
 * Cron reporting functions
 * function attendance_gettid ($vname, $name) {
 *     	  return $vid taxonomy term
 *
 * function attendance_reports () {  ... return $tid }  
 *       return taxonomy term name 
 */

/**----------------------------------------------------------------------------------- 
 *  attendance_reports
 */

function attendance_gettid ($vname, $name) {

  $voc = taxonomy_vocabulary_machine_name_load($vname);
  $query = new EntityFieldQuery;
  $result = $query
  ->entityCondition('entity_type', 'taxonomy_term')
  ->propertyCondition('name', $name)
  ->propertyCondition('vid', $voc->vid)
  ->execute();
  if ($result) {   
    $keys = array_keys($result['taxonomy_term']);
  } else {
    drupal_set_message(t('Error: attendance_gettid (includes): Taxonomy term not found -- vname: '. $vname .'; name: '.$name));
    return '';
  }
  return $keys[0];
}

function attendance_getstatistic ($name) {
  $tids = array();
  $voc = taxonomy_vocabulary_machine_name_load('attendance');
  $query = new EntityFieldQuery;
  $result = $query
  ->entityCondition('entity_type', 'taxonomy_term')
  ->propertyCondition('vid', $voc->vid)
  ->fieldCondition('field_statistic','value',$name,'=')
  ->execute();
  if ($result) {
    $keys = array_keys($result['taxonomy_term']);
  } else {
    drupal_set_message(t('Error: attendance_gettid (includes): Taxonomy term not found -- vname: '. $vname .'; name: '.$name));
    return '';
  }
  return $keys;
}

/**
 * attendance_reports
 * 1) Get all the configuration record(s)
 * 1) IF there's a missed-meeting message, cycle through attendance records and send 
 */

function attendance_reports () {
  $config = attendance_get_config();
  $msg = field_fetch_field_values('node',$config,'field_missedyoumessage');  $missedyou = $msg[0];
  if (isset($missedyou) && !empty($missedyou)) {
     check_attendance ($config);   // check attendance here only if "missed you" message is present
  }
}

/**-----------------------------------------------------------------------------------
 *  check_attendance   
 *  Run through members keyed to this config, and send messages as appropriate for missed meetings
 *  It is not necessary to process attendance in this way for attended, excused or makeup meetings
 */
          
function check_attendance($config) {
  $tids = attendance_getstatistic ('Absent');
  $query = new EntityFieldQuery;				// set up the query
  $result = $query
        ->entityCondition('entity_type', 'node')
        ->entityCondition('bundle', 'attendance')
        ->fieldCondition('field_processed','value','Yes','<>')	// hasn't been processed
        ->fieldCondition('field_attendance','tid',$tids,'=')	// attendance tid = the "absent" term from the config
        ->execute();
  if (isset($result['node'])) {
     // 1) Retrieve the user node for this attendance record
     // 2) Send out an email
     // 3) Update this node as processed with today's date if we're not in "test" mode
      $attendance_nids = array_keys($result['node']);
      $attendance_items = entity_load('node', $attendance_nids);
      foreach ($attendance_items as $node) {
        $uid = $node->field_uid['und'][0]['target_id'];
        $user = user_load($uid);
        if (!$user) {
		print "user $uid not loaded";
		exit;
	}
        $ary = field_fetch_field_values('node',$config,'field_test');  $test = $ary[0];
        $testemail = $config->field_test_email_address['und'][0]['email'];
        $from = $config->field_attendanceadmin['und'][0]['email'];
        $ary = field_fetch_field_values('node',$config,'field_missed_subject');  $subject = $ary[0];
        $ary = field_fetch_field_values('node',$config,'field_missedyoumessage');  $body = html_entity_decode($ary[0]);
        $ary = field_fetch_field_values('node',$config,'field_appendpolicy');  $append = $ary[0];
        if ($append == 'Yes') {
          $ary = field_fetch_field_values('node',$config,'body');  $body= $ary[0] . "\n\n" . $body;
        }
        $to = $user->mail;
        if ($test == 'Yes') {
          $body = "TEST TO: $to\n\n" . $body;
          $to = $testemail;
        }
        $cc = '';
	$bcc = '';
        attendance_email ($to, $from, $bcc, $cc, $subject, $body);

        // 1) Send an email to the admnistrator list; modify subject and body slightly
	if ($test <> 'Yes') {
	   $body = "COPY SENT TO ADMINISTRATOR for $to\n\n" . $body;
	   $subject = "(cpy) " . $subject;
	   $to = $config->field_attendanceadmin['und'][0]['email'];
	   $cc = '';
	   $bcc = '';
	   $i = 1;
	   while (isset($config->field_attendanceadmin['und'][$i]['email'])) {
		if ($i > 1) {
		  $cc = $cc . ',';
		}
		$cc = $cc . $config->field_attendanceadmin['und'][$i]['email'];
		$i++;
	  }
          attendance_email ($to, $from, $bcc, $cc, $subject, $body);
	}

	// 2) Update this node so it doesn't get processed/reminded again
	if ($test <> 'Yes') {
           $node->field_process_date['und'][0]['value'] = date('Y-m-d');
	   $node->field_processed['und'][0]['value'] = 'Yes';
    	   node_save($node);
        }
      }
  }
}

/**----------------------------------------------------------------------------------- 
 *  attendance_msg 
 *  Display message and return
 */

function attendance_msg($msg) {
 drupal_set_message(t($msg));
 return;
}

/**----------------------------------------------------------------------------------- 
 * A very simple hook_mail() implementation, for module "mymodule".
 * This implementation expects to receive a $body, $subject, and $headers 
 * inside the $params sent from drupal_mail(), rather than construct
 * those here in the hook.
 * @see drupal_mail()
 */
function attendance_mail($key, &$message, $params) {
  if (isset($params['subject'])) {
    $message['subject'] = $params['subject'];
  } 
  if (isset($params['body'])) {
    $message['body'][] = $params['body']; 
  }
  if (isset($params['headers']) && is_array($params['headers'])) {   
    $message['headers'] += $params['headers'];
  }
  
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

function attendance_email ($to, $from, $bcc, $cc, $subject, $body) {           // $msg is the message to be emailed - it could be to the member, to an admin, or a group of emails
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

  	$message = drupal_mail('attendance', 'message', $to, user_preferred_language($user), array('body' => $body, 'subject' => $subject, 'headers' => array('Cc' => $header_cc, 'Bcc' => $header_bcc)), $from, TRUE);
  	// You can check $message['result'] to see if the message sent successfully.
}
