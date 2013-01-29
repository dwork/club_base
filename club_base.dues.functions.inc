<?php

/**
  * dues_get_config : get configuration record for this run
  */
function dues_get_config ($chapter = NULL) {
  // If $chapter not passed, get it from the chapter function
  if (!isset($chapter)) {
     $chapter = club_base_get_chapter();
  }

  // Get the membership configuration record for this chapter. It drives all the reporting for all entities
  $query = new EntityFieldQuery;
  $result = $query
        ->entityCondition('entity_type', 'node')
        ->entityCondition('bundle', 'dues_policy')
        ->fieldCondition('field_chapter','target_id',$chapter,'=')
        ->execute();
  if (isset($result['node'])) {
    $config_nids = array_keys($result['node']);
    $configs = entity_load('node', $config_nids);
    foreach ($configs as $config) {
      return $config;
    }
  } else {
    drupal_set_message (t('Error: no dues policy for chapter $chapter found'));
    print (t('Error: no dues policy for chapter '.$chapter.' found'));
exit;
    return;
  }
}

function dues_gettid ($vname, $name) {

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
    drupal_set_message(t('Error: dues_gettid (includes): Taxonomy term not found -- vname: '. $vname .'; name: '.$name));
    return '';
  }
  return $keys[0];
}

?>
