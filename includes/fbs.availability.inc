<?php
/**
 * @file
 * Handles availability information from the library system.
 */

/**
 * Implements provider availability, items.
 */
function fbs_availability_items($provider_ids) {
  $record_ids = array();
  foreach ($provider_ids as $provider_id) {
    $record_ids[_fbs_get_record_id($provider_id)] = $provider_id;
  }
  $avails = fbs_service()->Catalog->getAvailability(fbs_service()->agencyId, array_keys($record_ids));
  $result = array();
  foreach ($avails as $item) {
    $provider_id = $record_ids[$item->recordId];
    $result[$provider_id] = array('available' => $item->available, 'reservable' => $item->reservable);
  }
  return $result;
}

/**
 * Implements provider availability, holdings.
 */
function fbs_availability_holdings($provider_ids) {
  $record_ids = array();
  foreach ($provider_ids as $provider_id) {
    $record_ids[_fbs_get_record_id($provider_id)] = $provider_id;
  }
  $holdings = fbs_service()->Catalog->getHoldings(fbs_service()->agencyId, array_keys($record_ids));
  $result = array();
  $tmp_issues = array();
  foreach ($holdings as $item) {
    $provider_id = $record_ids[$item->recordId];
    $result[$provider_id] = array(
      'reservable' => $item->reservable,
      // Gets properly set in the loop later.
      'available' => FALSE,
      'holdings' => array(),
      'total_count' => 0,
      // We don't know about this one.
      'is_internet' => FALSE,
      // And FBS can't tell us this.
      'reserved_count' => 0,
      'is_periodical' => FALSE,
    );


    $placement_parts = array('branch', 'department', 'location', 'sublocation');
    foreach ($item->holdings as $item_holding) {
      $result_holding = array(
        'available_count' => 0,
        'total_count' => 0,
        // We have no idea about this either.
        'reference_count' => 0,
        'placement' => array(),
      );

      // Add in placement.
      foreach ($placement_parts as $part) {
        if (!empty($item_holding->{$part}->title)) {
          $result_holding['placement'][] = $item_holding->{$part}->title;
        }
      }
      $placement_string = implode(' > ', $result_holding['placement']);
      // Count available materials.
      foreach ($item_holding->materials as $material) {
        $result[$provider_id]['total_count']++;
        $result_holding['total_count']++;
        if ($material->available) {
          $result_holding['available_count']++;
          $result[$provider_id]['available'] = TRUE;
        }

        if ($material->periodical) {
          // Add to issues.
          $key = (!empty($material->periodical->volumeYear) ?
                  $material->periodical->volumeYear : '') . ':' .
               $material->periodical->volume . ':' .
               (!empty($material->periodical->volumeNumber) ?
               $material->periodical->volumeNumber : '') . ':' .
               $placement_string;
          if (isset($tmp_issues[$key])) {
            $tmp_issues[$key]['available'] += $material->available ? 1 : 0;
            $tmp_issues[$key]['total'] += 1;
          }
          else {
            $tmp_issues[$key] = array(
              'vol' => $material->periodical->volume . (!empty($material->periodical->volumeNumber) ? '-' . $material->periodical->volumeNumber : ''),
              'year' => $material->periodical->volumeYear,
              'available' => $material->available ? 1 : 0,
              'total' => 1,
              'placement' => $placement_string,
              'local_id' => _fbs_periodical_get_local_id($item->recordId, $material->periodical),
            );
          }
        }
      }

      $result[$provider_id]['holdings'][] = $result_holding;
    }

    // Add issues in the right nesting.
    if (!empty($tmp_issues)) {
      $issues = array();
      $result[$provider_id]['is_periodical'] = TRUE;
      foreach ($tmp_issues as $issue) {
        if (!isset($issues[$issue['year']])) {
          $issues[$issue['year']] = array();
        }

        $year = &$issues[$issue['year']];

        if (!isset($year[$issue['vol']])) {
          $year[$issue['vol']] = array(
            'local_id' => $issue['local_id'],
            'placement' => array(),
          );
        }
        $year[$issue['vol']]['placement'] = array(
          'reservable' => $item->reservable,
          'available_count' => $issue['available'],
          'location' => $issue['placement'],
          'total_count' => $issue['total'],
        );
      }
      $result[$provider_id]['issues'] = $issues;
    }

  }
  return $result;
}
