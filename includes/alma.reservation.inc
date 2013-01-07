<?php

/**
 * @todo
 * Where do the user enter the interest periode and preferred branch
 * information. (THIS IS NOT IMPLEMENTED YET, SO DEFAULT IS USED)
 */



function alma_reservation_options($type, $account, $reservables) {
  /*
   * While we use ding_reservation for branch handling, we don't want to
   * depend on the module, as it would mean that ding_reservation can't be
   * disabled. So instead we throw an exception here.
   */

  if (!module_exists('ding_reservation')) {
    throw new Exception('Alma reservation options requires ding_reservation.');
  }

  $form = array();
  $profile = ding_user_provider_profile($account);

  $branches =  alma_reservation_pickup_branches($account);
  // @todo; use local branches from issue - some migth not be available in all branches
  $periods = array(
    7 => '1 week',
    14 => '2 weeks',
    21 => '3 weeks',
    30 => '1 month',
    60 => '2 months',
    90 => '3 months',
    180 => '6 months',
  );

  if(ding_user_is_provider_user($account)) {
    if(isset($profile->field_alma_preferred_branch))
      $preferred_branch = $profile->field_alma_preferred_branch[field_language('profile2', $profile, 'field_alma_preferred_branch')][0]['value'];
    else
      $preferred_branch = NULL;

    if(isset($profile->field_alma_interest_period))
      $preferred_period = $profile->field_alma_interest_period[field_language('profile2', $profile, 'field_alma_interest_period')][0]['value'];
    else
      $preferred_period = NULL;
  }
  else {
    $preferred_branch = NULL;
    $preferred_period = NULL;
  }

  // $collection = ting_collection_load($reservables->entity->ding_entity_id);
  // $reserve_id = array();
  // if(!empty($collection)) {
    // $options = array();
    // foreach ($collection->getEntities() as $entity) {
      // $options[$entity->ding_entity_id] = $entity->type;
    // }
    // $reserve_id = array('reserve_id' => array(
      // '#type' => 'select',
      // '#title' => t('Format'),
      // '#options' => $options
    // ));
  // }


  // $form += $reserve_id;
  $form += ding_reservation_default_options_branch($type, 'alma_preferred_branch', $preferred_branch, $branches);
  $form += ding_reservation_interest_period_selector($type, 'alma_interest_period', $preferred_period, $periods);

  return $form;
}



/**
 * Set preferred pickup branch
 */
function alma_reservation_set_preferred_branch($account, $branch) {
  // Throws an exception if we are not logged in.
  $creds = ding_user_get_creds($account);

  /**
   * @todo Should call alma an update the branch in the library system (not sure
   * if branch has the right format).
   */
  //alma_client_invoke('get_reservations', $creds['name'], $creds['pass'], $branch);
}

/**
 * Get list of reserved items.
 *
 * @param $account
 *  A user account
 *
 * @return array
 *  An array of DingProviderReservation objects
 *
 * @see ding_provider.reservation.inc
 */
function alma_reservation_list($account) {
  $creds = ding_user_get_creds($account);
  $reservations = alma_reservation_get_reservations($account);
  $result = array();

  // Create DingProviderReservation objects into to categories base on pickup
  // status.
  foreach ($reservations as $reservation) {
    // Setup base data.
    $data = array(
      'order_id' => $reservation['id'],
      'organisation_Id' => $reservation['organisation_id'],
      'ding_entity_id' => variable_get('ting_agency', '') . ':' . $reservation['record_id'],
      'pickup_branch_id' => $reservation['pickup_branch'],
      'created' => $reservation['create_date'],
      'expiry' => $reservation['valid_to'],
      'can_delete' => $reservation['deletable'],
      'queue_number' => $reservation['queue_number'],
      'status' => $reservation['status'],
    );
    // Add status specific data
    if (isset($reservation['pickup_number'])) {
      $data['pickup_date'] = $reservation['pickup_expire_date'];
      $data['ready_for_pickup'] = 1;
      $data['pickup_number'] = $reservation['pickup_number'];
    }
    else {
      $data['ready_for_pickup'] = 0;
    }
    // Create new Ding Provider Reservation object
    $result[$reservation['id']] = new DingProviderReservation($reservation['id'], $data);
  }

  return $result;
}


 /**
 * Create a reservation for a given account.
 */
function alma_reservation_create($account, $id, $branch, $expiry=null) {
  $creds = ding_user_get_creds($account);
  // Check if the users has this reservation and throw exception.
  //if (alma_reservation_exists($creds, $id)) {
  if( alma_reservation_exists($account, $id) ){
    throw new DingProviderReservationExists();
  }

  $profile2 = profile2_load_by_user($account,'provider_alma');
  if( !empty($profile2) ) {
    // interest period
    if( !isset($expiry) ) {
      // get interest period from profile2
      $interest_period = alma_get_interest_period($profile2);
      if( $interest_period ){
        $expiry = REQUEST_TIME + ($interest_period['key'] * 24 * 60 * 60 );
      }
    }
    if( !isset($expiry) ) {
      $expiry = REQUEST_TIME + DING_RESERVATION_DEFAULT_INTEREST_PERIOD;
    }
    
    // preferred branch
    if( empty($branch) ) {
      $pref_branch = alma_get_preferred_branch($profile2);
      if( $pref_branch ){
        $branches = alma_reservation_pickup_branches();
        foreach($branches as $key => $val){
          if( $val == $pref_branch ) {
            $branch = $key;
            break;
          }
        }
      }
    }
  }

  // Build the reservation parameters to send.
  $params = array(
    'id' => $id,
    'valid_from' => date('Y-m-d'),
    'valid_to' => alma_reservation_format_date($expiry),
    'pickup_branch' => $branch,
  );

  alma_reservation_clear_cache();
  // Try to make the reservation.
  $result = alma_client_invoke('add_reservation', $creds['name'], $creds['pass'], $params);

  if ($result === ALMA_AUTH_BLOCKED) {
    /**
     * @todo return better exception that informs the user about the block
     * status.
     */
    throw new DingProviderReservationNotAllowed();
  }

  if (is_int($result)) {
    // Reset session cache
    alma_reservation_clear_cache();
    return array(
      'queue_number' => $result,
    );
  }
  else {
    throw new DingProviderReservationNotAvailable();
  }
}

/**
 * Update order, by defining new expiry date or pickup branch.
 */
function alma_reservation_update_order($account, $id, $pickup_branch, $expiry_date) {
  $creds = ding_user_get_creds($account);
  $reservations = alma_reservation_get_reservations($account);
  if (isset($reservations[$id])) {
    $changes = array(
      'valid_to' => alma_reservation_format_date($expiry_date),
      'pickup_branch' => $pickup_branch,
    );
    // Alma do not return a status.
    alma_client_invoke('change_reservation', $creds['name'], $creds['pass'], $reservations[$id], $changes);
    alma_reservation_clear_cache();
    return TRUE;
  }

  return FALSE;
}

/**
 * Delete a reservation for a given account.
 */
function alma_reservation_delete($account, $id) {
  $creds = ding_user_get_creds($account);
  $reservations = alma_reservation_get_reservations($account);

  alma_reservation_clear_cache();
  if( isset($reservations[$id]) ) {
    return alma_client_invoke('remove_reservation', $creds['name'], $creds['pass'], $reservations[$id]);
  }
}

/**
 * Return a branch name for a given branch id.
 *
 * @param $branch_id String
 * @return String
 */
function alma_reservation_branch_name($branch_id) {
  // Get library organistation from alma.
  $organisation = alma_get_organisation();
  if (isset($organisation['branch'][$branch_id])) {
    return $organisation['branch'][$branch_id];
  }
  return NULL;
}

/**********************************
 * Helper functions
 */

function alma_reservation_get_reservations($account, $reset = FALSE) {
  $creds = ding_user_get_creds($account);

  // Try to look in the session for reservation information.
  if (!isset($_SESSION['alma_reservations']) || !is_array($_SESSION['alma_reservations']) || $reset) {
    $_SESSION['alma_reservations'] = alma_client_invoke('get_reservations', $creds['name'], $creds['pass']);
  }

  return $_SESSION['alma_reservations'];
}

function alma_reservation_clear_cache() {
  unset($_SESSION['alma_reservations']);
}

function alma_reservation_exists($account, $item_id) {
  $reservations = alma_reservation_get_reservations( $account );
  foreach ($reservations as $res) {
    if ($res['record_id'] == $item_id) {
      return TRUE;
    }
  }
  return FALSE;
}

function alma_reservation_format_date($datetime) {
  $timestamp = (!is_int($datetime)) ? strtotime($datetime) : $datetime;
  return date('Y-m-d', $timestamp);
}
