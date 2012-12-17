<?php
/**
 * @file
 * Implements the alma user concent interface. When a user gives permission to store his or her loan history for longer than 30 days
 * the user category is changed to reflect the new status. If the permission is revoked then the user category is changed back to the original.
 */

/**
 * Retrieve an array of user concent options
 * The return value is an array of consent options, containing a title and a description of the option.
 *
 * @return $alma_user_consent_info Array
 */
function alma_user_consent_info() {
  return array(
    'loan_history_store' => array(
      'title' => t('Personalisation'),
      'description' => t('In order to use this function, we request your permission to store your personal information'),
      'required' => TRUE,
    ),
  );
}

/**
 * Update the user concent
 * The return value is TRUE or FALSE depending on the success of the request.
 *
 * @param $account Object
 * @param $concent String
 * @param $previous_consents Boolean
 *
 * @return $status Boolean
 */
function alma_user_consent_update($account, $consent, $previous_consents) {
  switch($consent) {
    case 'loan_history_store':
      $creds = ding_user_get_creds($account);
      $patron = alma_get_patron($creds);
      if (isset($patron->category)) {
        // Changes mapped from the category structure in Axiell.
        $new_category = alma_user_consent_get_category_change($patron->category);
        // Alma do not return a status.
        $res = alma_client_invoke('change_user_consent', $creds['name'], $creds['pass'], $new_category);
        return TRUE;
      }
      break;
  }
  return FALSE;
}

/**
 * Delete the users concent.
 * The return value is TRUE or FALSE depending on the success of the request.
 *
 * @param $account Object
 * @param $concent String
 * @param $previous_consents Boolean
 *
 * @return $status Boolean
 */
function alma_user_consent_remove($account, $consent, $previous_consents) {
  switch($consent) {
    case 'loan_history_store':
      $creds = ding_user_get_creds($account);
      $patron = alma_get_patron($creds);
      if (isset($patron->category)) {
        // Changes mapped from the category structure in Axiell.
        $new_category = alma_user_consent_get_category_change($patron->category, TRUE);
        // Alma do not return a status.
        $res = alma_client_invoke('change_user_consent', $creds['name'], $creds['pass'], $new_category);
        return TRUE;
      }
      break;
  }
  return FALSE;
}

/**
 * Helper functions
 */

/**
 * Retrieve any consent record which the user has saved.
 * The return value is an array of consent arrays sorted by the date they were accepted.
 *
 * @param $account Object
 * @param $reset Boolean
 *
 * @return $alma_user_consent Array
 */
function alma_user_consent_get_consent($account) {
  $creds = ding_user_get_creds($account);
  $patron = alma_get_patron($creds);
  if(ding_user_is_provider_user($account)) {
    $consents = alma_user_consent_info();
    $return = array();
    foreach($consents as $id => $values) {
      $loaner_cats = alma_user_consent_get_loaner_categories();
      
      $return[$id] = (in_array($patron->category, $loaner_cats)) ? 1 : 0;
    }
    return $return;
  }
}

/**
 * Get the category which matches the current when changing.
 *
 * @param $category Number
 * @param $reset Boolean
 *
 * @return Number
 */
function alma_user_consent_get_category_change($category, $reset = FALSE) {
  $changes = alma_user_consent_get_loaner_categories();
  if($reset) {
    $changes = array_flip($changes);
  }
  return (isset($changes[$category])) ? $changes[$category] : 0;
}

/*
 * Temporary array of loaner categories with or without consent
 * This will soon be deprecated
 */
function alma_user_consent_get_loaner_categories() {
  return array(
    '01' => '71',
    '02' => '72',
    '04' => '74',
    '05' => '75',
    '07' => '24',
    '08' => '25',
    '09' => '26',
    '10' => '27',
    '19' => '28',
    '20' => '29',
  );
}