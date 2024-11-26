#include <fstream>
#include <iostream>
#include <thread>

#include "nlohmann/json.hpp"
#include "openxr/openxr.h"

struct ActionInfo {
  std::string name;
  XrActionType type;
  XrActionSet actionSet;
  XrAction action;
};

// JSON String to Char Array
void js_to_ca(const nlohmann::json &json, const std::string &string_name, char *out_cp_array, size_t ull_length) {
  if (out_cp_array == NULL) {
    return;
  }

  json[string_name].get<std::string>().copy(out_cp_array, ull_length);
}

int main() {
  std::ifstream f("actions.json");
  nlohmann::json j_file = nlohmann::json::parse(f);

  std::vector<std::string> vs_extensions = j_file["extensions"].get<std::vector<std::string>>();
  vs_extensions.insert(vs_extensions.end(), {XR_MND_HEADLESS_EXTENSION_NAME});

  XrInstance instance;
  {
    XrApplicationInfo app_info = {
        .applicationName = "barebones",
        .applicationVersion = 1,
        .engineName = "",
        .engineVersion = 1,
        .apiVersion = XR_MAKE_VERSION(1, 0, 0),
    };
    std::vector<const char *> vcp_extensions(vs_extensions.size(), nullptr);
    for (int i = 0; i < vs_extensions.size(); i++) {
      vcp_extensions[i] = vs_extensions[i].c_str();
    }
    XrInstanceCreateInfo instanceCreateInfo = {
        .type = XR_TYPE_INSTANCE_CREATE_INFO,
        .next = nullptr,
        .applicationInfo = app_info,
        .enabledApiLayerCount = 0,
        .enabledExtensionCount = static_cast<uint32_t>(vcp_extensions.size()),
        .enabledExtensionNames = vcp_extensions.data(),
    };
    if (XrResult result = xrCreateInstance(&instanceCreateInfo, &instance)) {
      std::cout << "Failed to create instance: " << result << std::endl;
      return 1;
    }
  }

  XrInstanceProperties instanceProperties = {XR_TYPE_INSTANCE_PROPERTIES};
  if (xrGetInstanceProperties(instance, &instanceProperties) == XR_SUCCESS) {
    std::cout << "Runtime name: " << instanceProperties.runtimeName << "\n";
    std::cout << "Runtime version: " << XR_VERSION_MAJOR(instanceProperties.runtimeVersion) << "."
              << XR_VERSION_MAJOR(instanceProperties.runtimeVersion) << "." << XR_VERSION_MAJOR(instanceProperties.runtimeVersion) << "\n";
  }

  XrSystemId systemId;
  XrSystemGetInfo systemGetInfo = {
      .type = XR_TYPE_SYSTEM_GET_INFO,
      .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY,
  };
  if (XrResult result = xrGetSystem(instance, &systemGetInfo, &systemId)) {
    std::cout << "Failed to get system: " << result << std::endl;
    return 1;
  }

  XrSession session;
  {
    XrSessionCreateInfo sessionCreateInfo = {
        .type = XR_TYPE_SESSION_CREATE_INFO,
        .next = nullptr,  // graphicsBinding here if not headless
        .systemId = systemId,
    };
    if (XrResult result = xrCreateSession(instance, &sessionCreateInfo, &session)) {
      std::cout << "Failed to create system: " << result << std::endl;
      return 1;
    }
  }

  // Action Setup
  std::vector<XrActionSet> v_action_sets(j_file["actionSets"].size());
  std::vector<XrActiveActionSet> v_active_action_sets(j_file["actionSets"].size());
  std::vector<ActionInfo> v_action_infos{};

  {
    std::unordered_map<std::string, std::vector<XrActionSuggestedBinding>> v_profile_suggested_bindings{};
    for (const auto &j_action_set : j_file["actionSets"]) {
      XrActionSetCreateInfo action_set_create_info = {
          .type = XR_TYPE_ACTION_SET_CREATE_INFO,
          .next = nullptr,
          .priority = j_action_set["priority"],
      };
      js_to_ca(j_file, "actionSetName", action_set_create_info.actionSetName, XR_MAX_ACTION_SET_NAME_SIZE);
      js_to_ca(j_file, "localizedActionSetName", action_set_create_info.localizedActionSetName, XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);

      XrActionSet action_set;
      if (XrResult result = xrCreateActionSet(instance, &action_set_create_info, &action_set)) {
        std::cout << "Create Action Set Error: " << result << std::endl;
        return 1;
      }
      v_action_sets.push_back(action_set);
      v_active_action_sets.push_back({
          .actionSet = action_set,
          .subactionPath = XR_NULL_PATH,
      });

      for (const auto &j_action : j_action_set["actions"]) {
        std::vector<XrPath> v_subaction_paths(j_action["subactionPaths"].size());

        for (int i = 0; i < v_subaction_paths.size(); i++) {
          const char *cp_subaction_path = j_action["subactionPaths"][i].get<std::string>().c_str();
          if (XrResult result = xrStringToPath(instance, cp_subaction_path, &v_subaction_paths[i])) {
            std::cout << "Failed to convert subaction path: " << cp_subaction_path << " - " << result << std::endl;
            return 1;
          }
        }

        XrActionCreateInfo action_create_info = {
            .type = XR_TYPE_ACTION_CREATE_INFO,
            .actionType = j_action["actionType"],
            .countSubactionPaths = (uint32_t)v_subaction_paths.size(),
            .subactionPaths = v_subaction_paths.data(),
        };
        js_to_ca(j_file, "actionName", action_create_info.actionName, XR_MAX_ACTION_NAME_SIZE);
        js_to_ca(j_file, "localizedActionName", action_create_info.localizedActionName, XR_MAX_LOCALIZED_ACTION_NAME_SIZE);

        XrAction action;
        if (XrResult result = xrCreateAction(action_set, &action_create_info, &action)) {
          std::cout << "Failed to create action: " << result << std::endl;
          return 1;
        }

        v_action_infos.push_back({
            .name = j_file["localizedActionName"],
            .type = action_create_info.actionType,
            .actionSet = action_set,
            .action = action,
        });

        // get all suggested bindings
        for (auto &j_profile_bindings : j_action["suggestedBindings"].items()) {
          const std::string &s_interaction_profile = j_profile_bindings.key();
          auto vs_bindings = j_profile_bindings.value().get<std::vector<std::string>>();

          auto &v_suggested_bindings = v_profile_suggested_bindings[s_interaction_profile];
          for (const auto &s_binding : vs_bindings) {
            XrPath path_binding;
            if (XrResult result = xrStringToPath(instance, s_binding.c_str(), &path_binding)) {
              std::cout << "Failed to convert binding path: " << s_binding << " - " << result << std::endl;
              return 1;
            }

            v_suggested_bindings.push_back({.action = action, .binding = path_binding});
          }
        }
      }
    }

    for (const auto &interaction_profile_bindings : v_profile_suggested_bindings) {
      XrPath path_interaction_profile;
      if (XrResult result = xrStringToPath(instance, interaction_profile_bindings.first.c_str(), &path_interaction_profile)) {
        std::cout << "Failed to convert interaction path: " << interaction_profile_bindings.first << " - " << result << std::endl;
        return 1;
      }

      XrInteractionProfileSuggestedBinding suggested_binding = {
          .type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
          .interactionProfile = path_interaction_profile,
          .countSuggestedBindings = static_cast<uint32_t>(interaction_profile_bindings.second.size()),
          .suggestedBindings = interaction_profile_bindings.second.data(),
      };
      if (XrResult result = xrSuggestInteractionProfileBindings(instance, &suggested_binding)) {
        std::cout << "Failed to suggest bindings: " << result << std::endl;
        return 1;
      }
    }
  }

  bool b_session_focused = false;

  XrEventDataBuffer event_data_buffer = {XR_TYPE_EVENT_DATA_BUFFER};
  while (true) {
    XrResult result_poll = xrPollEvent(instance, &event_data_buffer);
    while (result_poll == XR_SUCCESS) {
      switch (event_data_buffer.type) {
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
          const XrEventDataSessionStateChanged &session_state_changed = *reinterpret_cast<XrEventDataSessionStateChanged *>(&event_data_buffer);

          switch (session_state_changed.state) {
            case XR_SESSION_STATE_READY: {
              XrSessionActionSetsAttachInfo action_sets_attach_info = {
                  .type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
                  .countActionSets = static_cast<uint32_t>(v_action_sets.size()),
                  .actionSets = v_action_sets.data(),
              };
              if (XrResult result = xrAttachSessionActionSets(session, &action_sets_attach_info)) {
                std::cout << "Failed to attach action sets: " << result << std::endl;
                return 1;
              }

              XrSessionBeginInfo session_begin_info = {
                  .type = XR_TYPE_SESSION_BEGIN_INFO,
                  .next = nullptr,
              };
              if (XrResult result = xrBeginSession(session, &session_begin_info)) {
                std::cout << "Failed to begin session: " << result << std::endl;
              }

              break;
            }

            case XR_SESSION_STATE_FOCUSED: {
              b_session_focused = true;
              break;
            }

            default: {
              break;
            }
          }

          break;
        }

        default: {
          break;
        }
      }

      result_poll = xrPollEvent(instance, &event_data_buffer);
    }

    if (b_session_focused) {
      XrActionsSyncInfo action_sync_info = {
          .type = XR_TYPE_ACTIONS_SYNC_INFO,
          .countActiveActionSets = (uint32_t)v_active_action_sets.size(),
          .activeActionSets = v_active_action_sets.data()};

      if (XrResult result = xrSyncActions(session, &action_sync_info)) {
        std::cout << "Failed to sync actions: " << result << std::endl;
        return 1;
      }

      for (const auto &action_info : v_action_infos) {
        XrActionStateGetInfo action_state_get_info = {
            .type = XR_TYPE_ACTION_STATE_GET_INFO,
            .action = action_info.action,
            .subactionPath = XR_NULL_PATH,
        };
        switch (action_info.type) {
          case XR_ACTION_TYPE_BOOLEAN_INPUT: {
            XrActionStateBoolean state_boolean = {XR_TYPE_ACTION_STATE_BOOLEAN};
            if (XrResult result = xrGetActionStateBoolean(session, &action_state_get_info, &state_boolean)) {
              std::cout << "Failed to get bool action: " << result << std::endl;
              return 1;
            }

            std::cout << action_info.name << ": " << state_boolean.currentState << std::endl;
            break;
          }

          default: {
            std::cout << "Unknown action type: " << action_info.type << std::endl;
          }
        }
      }
      std::cout << "=====" << std::endl;

      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  }
}