<?php
/**
 * @file
 * Holds ingest steps for the Islandora Serial solution pack.
 */

/**
 * Form to add and re-order intermediate children.
 *
 * @param array $form
 *   An array representing a form in Drupal.
 * @param array $form_state
 *   An array containing the Drupal form state.
 * @param string $type
 *   Where this form is being used, either 'ingest' or 'manage'.
 * @param AbstractObject $object
 *   An optional AbstractObject representing an object within Fedora.
 *
 * @return array
 *   A renderable array containing the Drupal form.
 */
function islandora_serial_object_manage_intermediates_form($form, &$form_state, $type, AbstractObject $object = NULL) {
  module_load_include('inc', 'islandora_serial_object', 'includes/utilities');
  form_load_include($form_state, 'inc', 'islandora_serial_object', 'includes/manage');
  if (isset($form_state['triggering_element'])) {
    $last_parent = end($form_state['triggering_element']['#parents']);
    // Need to go through and rebuild the table because AJAX changes do not
    // persist unless forced.
    if ($last_parent == 'add_intermediate_object' || $last_parent == 'next' || $last_parent == 'submit') {
      if ($type == 'ingest') {
        $root = 0;
      }
      else {
        $root = $object->id;
        $form_state['islandora_serial_object_intermediates']['manage_root'] = $root;
      }
      if (isset($form_state['values']['table_wrapper']['islandora_serial_object_intermediates'])) {
        // Update the objects in the form state before we go sort the existing.
        $form_intermediates = $form_state['values']['table_wrapper']['islandora_serial_object_intermediates'];
        $updated_intermediates = $form_state['islandora_serial_object_intermediates']['intermediates'];
        foreach ($form_state['islandora_serial_object_intermediates']['intermediates'] as $parent => $children) {
          foreach ($children as $child_id => $child) {
            if (isset($form_intermediates[$child['id']])) {
              $current_values = $updated_intermediates[$parent][$child_id];
              foreach ($form_intermediates[$child['id']] as $key => $fs_vals) {
                if ($key != 'pid') {
                  if ((isset($child[$key]) && $child[$key] != $fs_vals) || !isset($child[$key])) {
                    $current_values[$key] = $fs_vals;
                  }
                }
              }
              // Handle the parent id last.
              $parent_id = $form_intermediates[$child['id']]['pid'];
              if ($child['pid'] != $parent_id) {
                $current_values['pid'] = $parent_id;
                // Remove the element from its current position and move it
                // to its new one.
                unset($updated_intermediates[$parent][$child_id]);
                $updated_intermediates[$parent_id][$child_id] = $current_values;
              }
              else {
                // Update the existing values at the original position.
                $updated_intermediates[$parent][$child_id] = $current_values;
              }

            }
            else {
              // The item was removed. Perhaps implement this?
              unset($updated_intermediates[$child['id']]);
            }
          }
        }
        $form_state['islandora_serial_object_intermediates']['intermediates'] = $updated_intermediates;
        $existing_intermediates = islandora_serial_object_sort_intermediates($form_state['islandora_serial_object_intermediates'], $root);
      }
      else {
        $existing_intermediates = array(
          'last_id' => 0,
          'intermediates' => array(),
        );
      }
      if ($last_parent == 'add_intermediate_object') {
        // New objects will always be appended with no parent to start.
        $new_intermediate_id = $existing_intermediates['last_id'] + 1;
        $existing_intermediates['last_id'] = $new_intermediate_id;

        // Make sure all new items are appended on after the highest max weight
        // for ordering purposes.
        $map_function = function ($element) {
          $weight_map_function = function($element) {
            return $element['weight'];
          };
          return empty($element) ? -9999 : max(array_map($weight_map_function, $element));
        };
        $max_weight = empty($existing_intermediates['intermediates']) ? -9999 : max(array_map($map_function, $existing_intermediates['intermediates']));
        $existing_intermediates['intermediates'][$root][$new_intermediate_id] = array(
          'id' => $new_intermediate_id,
          'pid' => $root,
          'weight' => $max_weight + 1,
          'ingested' => FALSE,
        );
      }
      $form_state['islandora_serial_object_intermediates'] = $existing_intermediates;
    }
  }
  $form['#tree'] = TRUE;
  $form['table_wrapper'] = array(
    'title' => array(
      '#markup' => t('Create Intermediate Objects'),
      '#prefix' => '<h2>',
      '#suffix' => '</h2>',
    ),
    'description' => array(
      '#markup' => t("Intermediate objects form the structure of the serial. They may be nested and reordered using this form. Note that empty intermediates will not be ingested."),
    ),
    '#prefix' => '<div id="islandora-serial-object-intermediate-table-wrapper">',
    '#suffix' => '</div>',
    'add_intermediate_object' => array(
      '#type' => 'button',
      '#value' => t('Add another'),
      '#ajax' => array(
        'callback' => 'islandora_serial_object_add_intermediate_object',
        'wrapper' => 'islandora-serial-object-intermediate-table-wrapper',
      ),
      '#weight' => 10,
    ),
  );

  // Ingest steps will handle the submission button rendering, when managing
  // add in our own.
  if ($type == 'manage') {
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Update'),
      '#validate' => array('islandora_serial_object_manage_intermediates_form_validate'),
      '#submit' => array('islandora_serial_object_manage_intermediates_form_submit'),
    );
  }
  islandora_serial_object_build_intermediates($form['table_wrapper'], $form_state, $type, $object);
  return $form;
}

/**
 * AJAX callback for adding a new intermediate.
 *
 * @param array $form
 *   An array representing a form in Drupal.
 * @param array $form_state
 *   An array containing the Drupal form state.
 *
 * @return array
 *   The portion of the form to be rendered.
 */
function islandora_serial_object_add_intermediate_object($form, &$form_state) {
  return $form['table_wrapper'];
}

/**
 * Validation for the manage intermediates form.
 *
 * @param array $form
 *   An array representing a form in Drupal.
 * @param array $form_state
 *   An array containing the Drupal form state.
 */
function islandora_serial_object_manage_intermediates_form_validate(&$form, &$form_state) {
  $last_parent = end($form_state['triggering_element']['#parents']);
  if ($last_parent == 'submit' || $last_parent == 'next') {
    module_load_include('inc', 'islandora_serial_object', 'includes/utilities');
    // We need to trigger a form rebuild before validation so we can save
    // all of the fun AJAX changes that had occurred.
    $form_id = $form['#form_id'];
    $form = drupal_rebuild_form($form_id, $form_state, $form);
    if (isset($form_state['values']['table_wrapper']['islandora_serial_object_intermediates'])) {
      foreach ($form_state['values']['table_wrapper']['islandora_serial_object_intermediates'] as $id => $intermediate) {
        if (trim($intermediate['caption']) === '' && trim($intermediate['number']) === '') {
          if (isset($form_state['islandora_serial_object_intermediates']['intermediates'][$id])) {
            // Need to see if these children are empty as well.
            $empty_children = FALSE;
            foreach ($form_state['islandora_serial_object_intermediates']['intermediates'][$id] as $vals) {
              if (trim($vals['caption']) !== '' || trim($vals['number']) !== '') {
                $empty_children = TRUE;
                break;
              }
            }
            if ($empty_children) {
              // Because of the above form rebuild, form_set_error does not
              // append the error class on the correct elements.
              foreach ($form['table_wrapper']['islandora_serial_object_intermediates'][$id] as $field_name => &$field) {
                $fields_to_mark = array(
                  'caption',
                  'number',
                );
                if (in_array($field_name, $fields_to_mark)) {
                  if (!isset($field['attributes']['class'])) {
                    $field['#attributes']['class'] = array('error');
                  }
                  else {
                    $field['#attributes']['class'][] = 'error';
                  }
                }
              };
              form_error($form['table_wrapper']['islandora_serial_object_intermediates'][$id]['caption'], t('An empty intermediate may not have any populated children!'));
            }
          }
        }
      }
    }
  }
}

/**
 * Submit handler for the manage intermediates form.
 *
 * @param array $form
 *   An array representing a form in Drupal.
 * @param array $form_state
 *   An array containing the Drupal form state.
 */
function islandora_serial_object_manage_intermediates_form_submit(&$form, &$form_state) {
  if (isset($form_state['islandora_serial_object_intermediates']) && !empty($form_state['islandora_serial_object_intermediates']['intermediates'])) {
    module_load_include('inc', 'islandora_serial_object', 'includes/utilities');
    module_load_include('inc', 'islandora_serial_object', 'includes/serial_tree.batch');
    if ($form['#form_id'] == 'islandora_ingest_form') {
      $object = islandora_ingest_form_get_object($form_state);
      $serial_id = $object->id;
      $ingest = TRUE;
    }
    else {
      $serial_id = $form_state['islandora_serial_object_intermediates']['manage_root'];
      $object = islandora_object_load($serial_id);
      $ingest = FALSE;
    }
    // Go grab the stored default values so we can pass the type off to the
    // batch.
    $default_values = json_decode($object['SERIAL-LEVELS']->content, TRUE);
    $filtered_children = $form_state['islandora_serial_object_intermediates']['intermediates'];
    foreach ($form_state['islandora_serial_object_intermediates']['intermediates'] as $parent_id => $children) {
      foreach ($children as $id => $child) {
        if (trim($child['caption']) === '' && trim($child['number']) === '') {
          unset($filtered_children[$parent_id][$id]);
        }
        else {
          // Grab the type from the defined default values. Note that the depth
          // is always one greater in the default values structure than what is
          // stored on the child.
          $filtered_children[$parent_id][$id]['type'] = $default_values[$child['depth'] + 1]['type'];
        }
      }
    }
    $tree = islandora_serial_object_get_intermediates_as_tree($filtered_children, $serial_id, $ingest);
    if ($tree) {
      islandora_serial_object_serial_batch($tree);
    }
  }
}

/**
 * Multi-step form for intermediate crazyness.
 *
 * @param array $form
 *   An array representing the Drupal form.
 * @param array $form_state
 *   An array containing the Drupal form state.
 * @param string $pid
 *   The pid of the object we are currently on.
 *
 * @return array
 *   An array representing the form to be rendered.
 */
function islandora_serial_object_manage_steps_form($form, &$form_state, $pid) {
  module_load_include('inc', 'islandora_serial_object', 'includes/utilities');

  $object = islandora_object_load($pid);
  $children = islandora_serial_object_get_children($object->id);
  $children_exist = !empty($children);
  // User needs to define levels before anything else.
  if (!isset($object['SERIAL-LEVELS'])) {
    module_load_include('inc', 'islandora_serial_object', 'includes/intermediate_fields.form');
    $form = islandora_serial_object_create_fields_for_intermediates_form($form, $form_state, 'manage');
  }
  // Levels defined but no children and just didn't add levels.
  elseif (isset($object['SERIAL-LEVELS']) && isset($form_state['islandora_serial_object_created_levels']) && !$children_exist) {
    $form = islandora_serial_object_manage_intermediates_form($form, $form_state, 'manage', $object);
  }
  elseif (isset($object['SERIAL-LEVELS']) && !isset($form_state['islandora_serial_object_created_levels']) && !$children_exist) {
    // Show levels form for editing.
    module_load_include('inc', 'islandora_serial_object', 'includes/intermediate_fields.form');
    $form = islandora_serial_object_create_fields_for_intermediates_form($form, $form_state, 'manage');
  }
  // Children exist.
  else {
    $form = islandora_serial_object_manage_intermediates_form($form, $form_state, 'manage', $object);
  }
  return $form;
}

/**
 * Submit handler to rebuild our multi-step edit form.
 *
 * @param array $form
 *   An array representing the Drupal form.
 * @param array $form_state
 *   An array containing the Drupal form state.
 */
function islandora_serial_object_manage_steps_rebuild(&$form, &$form_state) {
  $form_state['rebuild'] = TRUE;
}
