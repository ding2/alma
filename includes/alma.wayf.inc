<?php
/**
 * @file
 * Implements functions used to log users into the system through WAYF.
 */

/**
 * Implements hook_wayf_login_credentials().
 *
 * Used to set the name and password to log into the library system after WAYF
 * login.
 */
function alma_wayf_login_credentials($attributes) {
  $hash = variable_get('wayf_hash', '');
  $credentials = array();

  if ($hash && !empty($attributes['schacPersonalUniqueID'])) {
    // Extract unique id form the WAYF attributes (the CPR number).
    preg_match('/\d{10}$/', reset($attributes['schacPersonalUniqueID']), $uniqueID);
    $credentials = array(
      'name' => reset($uniqueID),
      'pass' => $hash,
    );
  }
  else {
    // Set message that hash value was not found.
    watchdog('alma', 'The WAYF hash value has not been set in setting.php as $conf[\'wayf_hash\'] and WAYF login will fail.', WATCHDOG_ERROR);
  }

  return $credentials;
}

/**
 * Implements hook_wayf_attributes().
 *
 * Used to define which attribute to use as login name.
 */
function alma_wayf_attributes() {
  return array(
    'schacPersonalUniqueID' => array(
      'field' => 'CPR',
    ),
  );
}
