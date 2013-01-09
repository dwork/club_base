<?php

/**
  * network_accumulate ($uid,$type,$ary) {
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
function network_accumulate ($uid, $type, $ary) {
  $config = attendance_get_config();
  $user = user_load($uid);
  // Calculate beginning and ending months
  if (isset($ary['min']['year']) && isset($ary['min']['month']) && isset($ary['min']['day'])) { // don't override
    $min = $ary['min']['year'].'-'.$ary['min']['month'].'-'.$ary['min']['day'];
  } else {
  }
  if (isset($ary['max']['year']) && isset($ary['max']['month']) && isset($ary['max']['day'])) { // don't override
    $max = $ary['max']['year'].'-'.$ary['max']['month'].'-'.$ary['max']['day'];
  } else {
  }

  // determine the query type
  switch ($type) {
  case 'Out':
    $fielduid = 'field_referring_member';
    break;
  case 'In':
    $fielduid = 'field_referred_member';
    break;
  }

  // Get the requested attendance records records. 
  $query = new EntityFieldQuery;
  $query
        ->entityCondition('entity_type', 'node')
        ->entityCondition('bundle', 'referral_exchange')
        ->fieldCondition($fielduid,'target_id',$uid)
        ->fieldCondition('field_date','value',array($min,$max),'BETWEEN')
        ->count();
  $count = $query->execute();
  return $count;
}
