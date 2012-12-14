<?php
/**
 * Implements provider availability, holdings.
 */
function alma_availability_holdings($provider_ids) {
  $ids = join(',', $provider_ids);
  
  $details = alma_client_invoke('catalogue_record_detail', $ids);
  $org = alma_get_organisation();
  
  $result = array();
  if ($details && isset($details['records'])) {
    foreach ($details['records'] as $alma_id => $record) {
      $holding = array(
        'local_id' => $alma_id,
        'available' => ($record['available_count'] > 0),
        'reservable' => ($record['show_reservation_button']),
        'show_reservation_button' => $record['show_reservation_button'],
        'reserved_count' => (int) $record['reservation_count'],
        'deferred_period' => FALSE,
	      'is_periodical' => ($record['media_class'] == 'periodical'),
        'is_internet' => ($record['media_class'] == 'internet' || in_array($record['resource_type'], array('electronic', 'electronical'))),
      );
      
      $holding['location'] = alma_get_holding_location($record, $holding['is_periodical'], $org);
      $holding['holdings'] = _alma_get_holdings($details, $holding['is_periodical']);
      
      $result[$alma_id] = $holding;
    }
  }
  return $result;
}

/**
 * @param type $holdings; array containing holding informations
 * @param type $is_periodical; Boolean that indicates whether the shown record is a periodical or not
 * @return sum of all total_count in $holdings
 */
function _alma_count_total($holdings, $is_periodical) {
  $total = 0;
  if ($is_periodical) {
    foreach ($holdings as $year => $issues) {
      foreach ($issues as $issue) {
        foreach ($issue as $holding) {
          $total += $holding['total_count'];
        }
      }
    }
  }
  else {
    foreach ($holdings as $holding) {
      $total += $holding['total_count'];
    }
  }
  return $total;
}

/**
 * @param type $res; returned array from alma_client class
 * @param type $is_periodical; Boolean that indicates whether the shown record is a periodical or not
 * @return html to be shown. Returns FALSE if no data is received from alma_client class
 */
function _alma_get_holdings($res, $is_periodical = FALSE) {
  if(isset($res['records'])) {
    if($is_periodical){
      return _alma_set_holdings_periodical($res);
    }
    else {
      return _alma_set_holdings($res);
    }
  }
  else {
    return FALSE;
  }
}

  /**
   * set holdings for all kinds of material except periodicals
   * @param array $res
   * @return array $result; 
   */
function _alma_set_holdings($res) {
  $holdings = array();
  foreach ($res['records'] as $alma_id => $records) {
    foreach ($records['holdings'] as $holding) {
      $holdings[] = $holding;
    }
  }
  
  $result = _alma_set_table_html($holdings);
  return $result;
}

  /**
   * set holdings if material is periodical only
   * @param array $res
   * @return array $result 
   */
function _alma_set_holdings_periodical($res){
  $holdings = array();
  foreach ($res['records'] as $alma_id => $records) {
    foreach ($records['holdings'] as $holding => $issue_year) {
      foreach ($issue_year as $key) {
        $holdings = array_merge($holdings, $key);
      }
    }
  }
  
  $result = _alma_set_table_html($holdings);
  return $result;
}
  /**
   * Make the html-table
   * @params $h; holding information for a given material
   * @return html-table
   */
function _alma_set_table_html($h) {
  // set a classname for styling the table
  $variables['attributes'] = array(
    'class' => array(
      drupal_html_class('availability_holdings_table'),
    ),
  );
  // set table header
  $variables['header'] = array(
    'placement' => t('Placement'),
    'copies' => t('Copies'),
    'home' => t('At home'),
    'reference' => t('Not for loan'),
    'checked_out' => t('Checked out'),
    'reservations' => t('Reservations'),
  );
  // set table rows
  $variables['rows'] = _alma_set_rows($h);    

  return $variables;
}

  /**
   * set rows in table for given holdings
   * @param $h; holding information for a given material
   * @return array; 
   */
function _alma_set_rows($h) {
  $rows = array();
  $org = alma_get_organisation();
  
  $copies_total = 0;
  $home_total = 0;
  $reservations_total = 0;
  $reference_total = 0;
  $checked_out_total = 0;
  
  foreach ($h as $key => $data) {
    $row = array();
    $row['placement'] = $org['branch'][$data['branch_id']];
    
    if(!empty($data['department_id'])){
      $row['placement'] = $row['placement'] .' → '. $org['department'][$data['department_id']];
    }
    
    if(!empty($data['collection_id'])){
      $row['placement'] = $row['placement'] .' → '. $org['collection'][$data['collection_id']];
    }
    $row['copies'] = (int) $data['total_count'];
    $copies_total += $row['copies'];
    $row['home'] = (int) $data['available_count'];
    $home_total += $row['home'];
    $row['reference'] = (int) $data['reference_count'];
    $reference_total += $row['reference'];
    $row['checked_out'] = (int) $data['checked_out_count'];
    $checked_out_total += $row['checked_out'];
    $row['reservations'] = (int) $data['ordered_count'];
    $reservations_total += $row['reservations'];
    $rows[] = $row; 
  }
  
  if(count($rows) >= 1){
    $rows = _clean_up_rows($rows);
  }
  //Adding last row - totals
  $row = array();
  $row['data']['Library'] = t('Total');
  $row['data']['Copies'] = $copies_total;
  $row['data']['Home'] = $home_total;
  $row['data']['Reference'] = $reference_total;
  $row['data']['Checked_out'] = $checked_out_total;
  $row['data']['Reservations'] = $reservations_total;
  $row['class'] = array(drupal_html_class('availability_holdings_last_row'));
  $rows[] = $row;
  return $rows;
}

  /**
  * if the same placement exists several times collect them in one line
  * @param array
  * @return array;
  */
function _clean_up_rows($_rows) {
  $rows = array();
  $placements = array();
  
  foreach ($_rows as $row) {
    $currkey = $row['placement'];
    if(!in_array($currkey, $placements)){
      $placements[] = $currkey;
      $placementsarr = _get_placements_with_key($_rows, $currkey);
      $this_row = _sum_placement($placementsarr);
      $rows[] = $this_row;
    }
  }
  return $rows;
}

  /**
   * collect materials with the same placement
   * @param array $_rows
   * @param String $currkey
   * @return array $rows; 
   */
function _get_placements_with_key($_rows, $currkey){
  $rows = array();
  foreach ($_rows as $key) {
    if($key['placement'] == $currkey){
      $rows[] = $key;
    }
  }
  return $rows;
}
  /**
   * sum material for same placement in one row
   * @param $placementsarr; array with all instances of the same placement - ie. 'Hovedbiblioteket'
   * @return array; $row 
   */
function _sum_placement($placementsarr){
  $row = $placementsarr[0];
  for($i = 1; $i < count($placementsarr);$i++){
    $next_row = $placementsarr[$i];
    $row['copies'] += $next_row['copies'];
    $row['home'] += $next_row['home'];
    $row['reference'] += $next_row['reference'];
    $row['checked_out'] += $next_row['checked_out'];
    $row['reservations'] += $next_row['reservations'];
  }
  return $row;
}

function alma_get_holding_location($record, $is_periodical = FALSE, $org = NULL) {
  $holding_parts = array('branch', 'department', 'location', 'sublocation', 'collection');
  
  $parts = array();
  $result = array();
  $result['holdings'] = array();
  if($is_periodical) {
    foreach( $record['holdings'] as $volume => $issues ) { 
      foreach($issues as $issue_no => $holds){
        $issue = array(
          'branches' => array(),
        );
        foreach( $holds as $key => $branch_holding) {
          if( !in_array( $branch_holding['branch_id'], $issue['branches'] ) ) {
            $issue['branches'][] = $branch_holding['branch_id'];
          }

          $issue['local_id'] = $branch_holding['reservable'];
          $issue['reservable'] = (($branch_holding['status'] == 'availableForLoan') &&
            ((int) $branch_holding['total_count'] - (int) $branch_holding['reference_count']));
          $issues_array[$volume][$issue_no] = $issue;
          $parts = array();
          foreach ($holding_parts as $part) {
        		if (!empty($branch_holding[$part . '_id']) && !empty($org[$part][$branch_holding[$part . '_id']])) {
        		  $parts[] = $org[$part][$branch_holding[$part . '_id']];
        		}
          }
        
          if (!empty($branch_holding['shelf_mark'])) {
        		// Shelf mark might have leading >, strip any and replace the rest
        		// with the proper arrow.
        		$parts[] = strtr(trim($branch_holding['shelf_mark'], " >\n\t"), array('>' => ''));
          }
        
          $parts = array_filter($parts);
        
          if ($parts) {
        		$branch_string = join(' → ', $parts);
        		$result['holdings_available'][] = $parts;
        		if( !in_array($parts, $result['holdings']) ) {
        		  $result['holdings'][] = $parts; 
        		}
          }
        }
      }
    }
    if( is_array($result['holdings']) ) {
  	  asort($result['holdings']);
  	}
  	$result['issues'] = $issues_array;
  } // END periodicals
  else {
    foreach ($record['holdings'] as $branch_holding) {
      $parts = array();
  	  foreach ($holding_parts as $part) {
  	    if (!empty($branch_holding[$part . '_id'])) {
  	      $parts[] = $org[$part][$branch_holding[$part . '_id']];
  	    }
      }
  	  if (!empty($branch_holding['shelf_mark'])) {
  	    // Shelf mark might have leading >, strip any and replace the rest
  	    // with the proper arrow.
  	    $parts[] = strtr(trim($branch_holding['shelf_mark'], " >\n\t"), array('>' => '→'));
  	  }

      $parts = array_filter($parts);

  	  if ($parts) {
  	    $result['holdings'][] = $parts;
  	  }
    }
  }
  return $result;
}