<?php
/**
 * @file
 * Implements provider hooks that is associated with loans in alma.
 */

/**
 * Implements hook_loan_list().
 *
 * This functions finds all loans for a given user account and returns the loans
 * as DingProviderLoan objects.
 *
 */
function alma_loan_list($account) {
  $creds = ding_user_get_creds($account);
  $results = array();

  try {
    // Get all loans form alma for the user account given.
    $loans =  alma_client_invoke('get_loans', $creds['name'], $creds['pass']);
  } catch (Exception $e) {
    throw new DingProviderLoanUserError($e->getMessage());
  }
  
  foreach ($loans as $loan) {
    $results[$loan['id']] = new DingProviderLoan($loan['id'], array(
      'ding_entity_id' => variable_get('ting_agency', '') . ':' . $loan['record_id'],
      'loan_date' => $loan['loan_date'],
      'expiry' => $loan['due_date'],
      'renewable' => $loan['is_renewable'],
      'materials_number' => $loan['id'],
      'notes' => isset($loan['notes']) ? $loan['notes'] : DingEntityBase::NULL,
      'message' => isset($loan['message']) ? alma_loan_message($loan['message']) : DingEntityBase::NULL,
    ));
  }

  return $results;
}

/**
 * Implements hook_loan_renew().
 *
 * Tries to renew the loans with the id's passed as parameter.
 */
function alma_loan_renew($account, $ids) {
  $creds = ding_user_get_creds($account);
  $result = array();
  $res = alma_client_invoke('renew_loan', $creds['name'], $creds['pass'], $ids);
  foreach ($ids as $id) {
    if (isset($res[$id]) && $res[$id] === TRUE) {
      $result[$id] = array(
        'state' => DingProviderLoan::STATUS_RENEWED,
      );
    }
    else {
      $result[$id] = array(
        'state' => DingProviderLoan::STATUS_NOT_RENEWED,
        'message' => $res[$id],
      );
    }
  }
  return $result;
}

/**
 * Implements hook_loan_list().
 *
 * This functions finds all loans for a given user account and returns the loans
 * as DingProviderLoan objects.
 *
 */
function alma_loan_overdue($account) {
  $creds = ding_user_get_creds($account);
  $results = array();

  try {
    // Get all loans form alma for the user account given.
    $loans =  alma_client_invoke('get_overdue_loans', $creds['name'], $creds['pass']);
  } catch (Exception $e) {
    throw new DingProviderLoanUserError($e->getMessage());
  }

  foreach ($loans as $loan) {
    $results[$loan['id']] = new DingProviderLoan($loan['id'], array(
      'ding_entity_id' => variable_get('ting_agency', '') . ':' . $loan['record_id'],
      'loan_date' => $loan['loan_date'],
      'expiry' => $loan['due_date'],
      'renewable' => $loan['is_renewable'],
      'materials_number' => $loan['id'],
      'notes' => isset($loan['notes']) ? $loan['notes'] : DingEntityBase::NULL,
    ));
  }

  return $results;
}

/**
 * Return one or all of the possible loan messages.
 * In cases where renewing loans is not possible the system returns a reason message and 
 * this function converts that system message into a human readable message.
 *
 * @param string $message_key
 *  The message key returned by Alma, or null to retrieve the entire list.
 *
 * @return string or array
 *
 */
function alma_loan_message($message_key = null) {
  $messages = array(
    'isLoanedToday' => t('This has been loaned today'),
    'isRenewedToday' => t('This has been renewed today'),
    'isOverdue' => t('This is overdue'),
    'patronIsDeniedLoan' => t('You have been denied loans'),
    'patronHasDebt' => t('You have debt'),
    'maxNofRenewals' => t('The maximum loans has been reached'),
    'patronIsInvoiced' => t('You have invoices to payed'),
    'copyHasSpecialCircCat' => t('This copy has a special category'),
    'copyIsReserved' => t('This is reserved by another'),
    'renewalIsDenied' => t('It is not possible to renew'),
  );
  return ($message_key) ? $messages[$message_key] : $messages;
}