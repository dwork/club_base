<?php

/*
    field_fetch_field_values : get field values
    club_base_form_alter : system implementation of hook_form_alter
    club_base_get_chapter() : get default chapter for this user or from the base default variable
    club_base_user_name ($user) : Get formatted user name (first middle last suffix)
    club_base_get_phone ($uid) : Get formatted user phones
*/

/**
  * Implement hook_form_alter to override filters
  * If the filters aren't entered, default to the values in $config
  */
function club_base_form_alter(&$form, &$form_state, $form_id) {
  // Attendance views
  if ($form_id == 'views_exposed_form' && ($form['#action'] == '/attendance-summary-by-user' || $form['#action'] == '/attendance-detail')) {
    $config = attendance_get_config();
    if (empty($_GET['field_date_value']['min']['year']) && empty($_GET['field_date_value']['min']['month']) && empty($GET['field_date_value']['min']['day']))  {
      $form_state['input']['field_date_value']['min']['year'] = substr($config->field_policy_date_range['und'][0]['value'],0,4);
      if (substr($config->field_policy_date_range['und'][0]['value'],5,1) == '0') {
        $form_state['input']['field_date_value']['min']['month'] = substr($config->field_policy_date_range['und'][0]['value'],6,1);
      } else {
        $form_state['input']['field_date_value']['min']['month'] = substr($config->field_policy_date_range['und'][0]['value'],5,2);
      }
      if (substr($config->field_policy_date_range['und'][0]['value'],8,1) == '0') {
        $form_state['input']['field_date_value']['min']['day'] = substr($config->field_policy_date_range['und'][0]['value'],9,1);
      } else {
        $form_state['input']['field_date_value']['min']['day'] = substr($config->field_policy_date_range['und'][0]['value'],8,2);
      }
    }
    if (empty($_GET['field_date_value']['max']['year']) && empty($_GET['field_date_value']['max']['month']) && empty($GET['field_date_value']['max']['day']))  {
      $form_state['input']['field_date_value']['max']['year'] = substr($config->field_policy_date_range['und'][0]['value2'],0,4);
      if (substr($config->field_policy_date_range['und'][0]['value2'],5,1) == '0') {
        $form_state['input']['field_date_value']['max']['month'] = substr($config->field_policy_date_range['und'][0]['value2'],6,1);
      } else {
        $form_state['input']['field_date_value']['max']['month'] = substr($config->field_policy_date_range['und'][0]['value2'],5,2);
      }
      if (substr($config->field_policy_date_range['und'][0]['value2'],9,1) == '0') {
        $form_state['input']['field_date_value']['max']['day'] = substr($config->field_policy_date_range['und'][0]['value2'],9,1);
      } else {
        $form_state['input']['field_date_value']['max']['day'] = substr($config->field_policy_date_range['und'][0]['value2'],8,2);
      }
    }
  }
}

/**
 * Returns field values as actual entities where possible,
 * also allows selection of individual items to be returned
 */
function a_field_fetch_field_values($entity_type, $entity, $field_name, $get_delta = NULL, $get_key = NULL) {
  $values = array();
  if (isset($entity->$field_name) && !empty($entity->$field_name)) {
    foreach (field_get_items($entity_type, $entity, $field_name) as $delta => $item) {
      $value = $item;
      $keys = array_keys($item);
      if (count($keys)==1) {
        $key = $keys[0];
        switch ($key) {
          case 'nid':
            $value = array_shift(entity_load('node', array($item[$key])));
            break;
          case 'uid':
            $value = array_shift(entity_load('user', array($item[$key])));
            break;
          case 'tid':
            $value = array_shift(entity_load('taxonomy_term', array($item[$key])));
            break;
          case 'vid':
            $value = array_shift(entity_load('taxonomy_vocabulary', array($item[$key])));
            break;
          case 'value':
            $value = $item['value'];
            break;
        }
      }
      else {
        if ($get_key && isset($item[$get_key])) {
          $value = $item[$get_key];
        }
        elseif (array_key_exists('value', $item)) {
          $value = isset($item['safe_value']) ? $item['safe_value'] : $item['value'];
        }
      }
      $values[$delta] = $value;
    }
  }
  if (is_numeric($get_delta)) {  
    return isset($values[$get_delta]) ? $values[$get_delta] : NULL;
  }
  return $values;
}

/*
 * club_base_get_chapter
 * function to retrieve chapter based on logged in user
 */
function club_base_get_chapter() {
  global $user;
  $node = user_load($user->uid);

  // If the user's chapter is set, return it
  if (isset($node->field_chapter['und'][0]['target_id'])) {  // logged in user
    return $node->field_chapter['und'][0]['target_id'];

  // else if the default chapter is set, return it
  } else {
    $chapter = variable_get('club_default_chapter');
    if (isset($chapter)) {
      return $chapter;
    }
  }

  // No user chapter & no default; so, get the first Chapter/club record
  $query = new EntityFieldQuery();
  $query->entityCondition('entity_type', 'node')
            ->entityCondition('bundle', 'chapter');
  $result = $query->execute();
  if (isset($result['node'])) {
    return current(array_keys($result['node']));
  }
}


/**
 * Return concatenated user names, given the user profile
 */
function club_base_user_name ($user) {
  // first ' ' middle ' ' last ' ' suffix
  $name = '';

  // First Name
    if (isset($user->field_first_name['und'])) {
       $name .= $user->field_first_name['und'][0]['value'] . ' ';
    }

  // Middle Name
    if (isset($user->field_middle_initial['und'])) {
       $name .= $user->field_middle_initial['und'][0]['value'] . ' ';
    }

  // Last Name
    if (isset($user->field_last_name['und'])) {
       $name .= $user->field_last_name['und'][0]['value'] . ' ';
    }

  // Suffix
    if (isset($user->field_suffix['und'])) {
       $name .= $user->field_suffix['und'][0]['value'] . ' ';
    }
  
  return $name;
}

/**
 * Retreive field collection item "phone" for a particular user
 * returns an array of phone numbers including the phone, the type and the location
 */
function club_base_get_phone ($uid) {

  $profile = profile2_load_by_user($uid,'main');
  $fieldset = 'field_contact_phone';
  $key1 = 'field_phone';
  $key2 = 'field_phone_type';
  $key3 = 'field_phone_location';

  $phones = field_get_items('profile2', $profile, $fieldset);

  $ph = array();
  if (!empty($phones)) {
    foreach ($phones as $phone) {
      if (!empty($phone['value'])) {
        $field_collection = field_collection_field_get_entity($phone['value']);
        if ($field_collection) {
          $line = array();

          $items = field_get_items('field_collection_item', $field_collection, $key1);
          if ($items) {
             $item = reset($items);
             $number = substr($item['number'],0,3).'-'.substr($item['number'],3,3).'-'.substr($item['number'],6,4);
             $line['number'] = substr($item['number'],0,3).'-'.substr($item['number'],3,3).'-'.substr($item['number'],6,4);
          }

          $items = field_get_items('field_collection_item', $field_collection, $key2);
          if ($items) {
             $item = reset($items);
             $line['type'] = ' '.taxonomy_term_load($item['tid'])->name;
          }

          $items = field_get_items('field_collection_item', $field_collection, $key3);
          if ($items) {
             $item = reset($items);
             $line['location'] = ' '.taxonomy_term_load($item['tid'])->name;
          }
        }
        $ph[] = $line;
      }
    }
  }
  return $ph;
}