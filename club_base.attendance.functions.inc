<?php

/**
  * attendance_get_config : get configuration record for this run
  */
function attendance_get_config ($chapter = NULL) {
  // If $chapter not passed, get it from the chapter function
  if (!isset($chapter)) {
     $chapter = club_base_get_chapter(); 
  }

  // Get the attendance configuration record for this chapter. It drives all the reporting for all entities
  $query = new EntityFieldQuery;
  $result = $query
        ->entityCondition('entity_type', 'node')
        ->entityCondition('bundle', 'attendance_configuration')
        ->fieldCondition('field_chapter','target_id',$chapter,'=')
        ->execute();
  if (isset($result['node'])) {
    $config_nids = array_keys($result['node']);
    $configs = entity_load('node', $config_nids);
    foreach ($configs as $config) {
      return $config;
    }
  } else {
    drupal_set_message (t('Error: no attendance configuration record for chapter $chapter found'));
    print (t('Error: no attendance configuration record for chapter '.$chapter.' found'));
exit;
    return;
  }
}

/**-----------------------------------------------------------------------------------
 *  attendance_tid_gettid
 *  Retrieve the tid given the Vocabulary machine name and TID term name
 */

function attendance_tid_gettid ($vname, $name, $operator = '=') {

  $voc = taxonomy_vocabulary_machine_name_load($vname);
  $query = new EntityFieldQuery;
  $result = $query
  ->entityCondition('entity_type', 'taxonomy_term')
  ->propertyCondition('vid', $voc->vid)
  ->fieldCondition('field_statistic', 'value', $name, $operator)
  ->execute();
  if ($result) {
    $keys = array_keys($result['taxonomy_term']);
  } else {
    $keys = attendance_gettid($vname,$name);
    if (!isset($keys)) {
      drupal_set_message(t('Error: attendance_tid_gettid (includes): Taxonomy term not found -- vname: '. $vname .'; name: '.$name));
      return '';
    }
  }
  return $keys;
}


/**
  * attendance_set_date_filter ();
  */
function attendance_set_date_filter() {
  $config = attendance_get_config();
  $field_date_value['min']['year'] = '2011';
  $field_date_value['max']['year'] = '2012';
  return $field_date_value;
}

/**
  * attendance_accumulate ($uid,$type,$ary) {
  * $uid -- get user node
  * $type -- attendance type or "Total" (present, absent, makeup, excused)
  * $ary -- $ary->field_date_value['min'][year, month, day] to $ary->field_date_value['max'][year, month, day]
  *         if !min then use begining value from $config
  *         if !min.year then use beginning year from $config
  *         if !min.month then use beginning month from $donfig
  *         if !min.day then use 1 
  *         if !max then use ending value from $config
  *         if !max.year then use ending year from $config
  *         if !max.month then use ending month from $donfig
  *         if !max.day then use last day of selected month 
  * $config's date range: field_policy_date_range['und'][0][value] -> YYYY-MM-DD 00:00:00  beginning date    
  *                       field_policy_date_range['und'][0][value2] -> YYYY-MM-DD 00:00:00 ending date
  */
function attendance_accumulate ($uid, $type, $ary) {
  $config = attendance_get_config();
  $user = user_load($uid);
  // Calculate beginning and ending months
  if (isset($ary['field_date_value']['min']['year']) && isset($ary['field_date_value']['min']['month']) && isset($ary['field_date_value']['min']['day'])) { // don't override
    $min = $ary['field_date_value']['min']['year'].'-'.$ary['field_date_value']['min']['month'].'-'.$ary['field_date_value']['min']['day'];
  } else {
    $min = substr($config->field_policy_date_range['und'][0]['value'],0,10);
  }
  if (isset($ary['field_date_value']['max']['year']) && isset($ary['field_date_value']['max']['month']) && isset($ary['field_date_value']['max']['day'])) { // don't override
    $max = $ary['field_date_value']['max']['year'].'-'.$ary['field_date_value']['max']['month'].'-'.$ary['field_date_value']['max']['day'];
  } else {
    $max = substr($config->field_policy_date_range['und'][0]['value2'],0,10);
  }

  // if other than "Total", get the TID for this query
  if ($type <> 'Total') {
    $tids = attendance_tid_gettid ('Attendance', $type);
  } else {
    $tids = attendance_tid_gettid ('Attendance', 'Do Not Count', '<>');
  }

  // Get the requested attendance records records. 
  $query = new EntityFieldQuery;
  $query
        ->entityCondition('entity_type', 'node')
        ->entityCondition('bundle', 'attendance')
        ->fieldCondition('field_uid','target_id',$uid)
        ->fieldCondition('field_attendance','tid', array($tids), 'IN')
 	->fieldCondition('field_date','value',array($min,$max),'BETWEEN') 
        ->count();
  $totalcount = $query->execute();

  // If we're not counting makeups (e.g. if they're used to annotate other types of non-counting meetings), remove count from total
  if ($type == 'Total' && $config->field_makeupsallowed['und'][0]['value'] <> 'Yes') {
    $tids = attendance_gettid ('Attendance', 'Makeup');
    // Get the requested attendance records records.
    $query = new EntityFieldQuery;
    $query
        ->entityCondition('entity_type', 'node')
        ->entityCondition('bundle', 'attendance')
        ->fieldCondition('field_uid','target_id',$uid)
        ->fieldCondition('field_attendance','tid', array($tids), 'IN')
 	->fieldCondition('field_date','value',array($min,$max),'BETWEEN') 
        ->count();
    $makeups = $query->execute();
    $totalcount = $totalcount - $makeups;
  }

  return $totalcount;
}
