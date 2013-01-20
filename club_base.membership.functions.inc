<?php

/**
  * membership_get_config : get configuration record for this run
  */
function membership_get_config ($chapter = NULL) {
  // If $chapter not passed, get it from the chapter function
  if (!isset($chapter)) {
     $chapter = club_base_get_chapter();
  }

  // Get the membership configuration record for this chapter. It drives all the reporting for all entities
  $query = new EntityFieldQuery;
  $result = $query
        ->entityCondition('entity_type', 'node')
        ->entityCondition('bundle', 'membership_policy')
        ->fieldCondition('field_chapter','target_id',$chapter,'=')
        ->execute();
  if (isset($result['node'])) {
    $config_nids = array_keys($result['node']);
    $configs = entity_load('node', $config_nids);
    foreach ($configs as $config) {
      return $config;
    }
  } else {
    drupal_set_message (t('Error: no membership policy for chapter $chapter found'));
    print (t('Error: no membership policy for chapter '.$chapter.' found'));
exit;
    return;
  }
}

?>
