# openxr-actions-tester

Easy way to query and test OpenXR input.

## Usage

Create a JSON file like the ones found in `examples/` and name it `actions.json` in the working directory of the app.

## Example

```json
{
  "extensions": [],
  "actionSets": [
    {
      "actionSetName": "set_1",
      "localizedActionSetName": "Action Set 1",
      "priority": 0,
      "actions": [
        {
          "actionName": "test_up",
          "actionType": 1,
          "subactionPaths": [],
          "localizedActionName": "Test UP",
          "suggestedBindings": {
            "/interaction_profiles/oculus/touch_controller": [
              "/user/hand/left/input/y/click",
              "/user/hand/right/input/b/click"
            ]
          }
        },
        {
          "actionName": "test_down",
          "actionType": 1,
          "subactionPaths": [],
          "localizedActionName": "Test DOWN",
          "suggestedBindings": {
            "/interaction_profiles/oculus/touch_controller": [
              "/user/hand/left/input/x/click",
              "/user/hand/right/input/a/click"
            ]
          }
        }
      ]
    }
  ]
}
```