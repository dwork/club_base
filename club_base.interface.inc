<?php

/*
 * club_base.interface.inc
 *
 */

/*
 * club_base_interface
 * Cron interface jobs
 */
function club_base_interface () {
}

/*
 * club_base_get_chapter
 * function to retrieve chapter based on logged in user
 */
function ___club_base_get_chapter() {
  global $user;
  $node = user_load($user->uid);
dsm($node);
  if (isset($node->field_chapter['und'][0]['target_id'])) {  // logged in user
    return $node->field_chapter['und'][0]['target_id'];
  } elseif (function_exists('attendance_get_config')) {  // attendance module installed
    $config = attendance_get_config();
    if (isset($config->field_chapter['und'][0]['target_id'])) {
      return $config->field_chapter['und'][0]['target_id'];
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
