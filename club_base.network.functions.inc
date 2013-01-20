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

  // determine the query type
  switch ($type) {
  case 'Given':
    $fielduid = 'field_referring_member';
    $file = 'referral_exchange';
    break;
  case 'Received':
    $fielduid = 'field_referred_member';
    $file = 'referral_exchange';
    break;
  case 'Given$':
    $fielduid = 'field_referring_member';
    $file = 'business_exchange';
    break;
  case 'Received$':
    $fielduid = 'field_referred_member';
    $file = 'business_exchange';
    break;
  case 'M2M':
    $fielduid = 'field_members';
    $file = 'm2m';
    break;
  }
  // Get the requested attendance records records. 
  $query = new EntityFieldQuery;
  $query
        ->entityCondition('entity_type', 'node')
        ->entityCondition('bundle', $file)
        ->fieldCondition($fielduid,'target_id',$uid,'=')
        ->fieldCondition('field_date','value',array($min,$max),'BETWEEN');
  $result = $query->execute();
  $count = 0;
  if ($file == 'business_exchange') {
    $value = 0;
    if (isset($result['node'])) {
      $keys = array_keys($result['node']);
      $nodes = node_load_multiple($keys);
      foreach ($nodes as $node) {
        $value = $value + $node->field_value['und'][0]['value'];
      }
      $count = count(array_keys($result['node']));
    }
    return array($count,$value);
  } else {
    if (isset($result['node'])) {
      $count = count(array_keys($result['node']));
    }
    return $count;
  }
}
