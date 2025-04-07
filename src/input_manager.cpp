#include "input_manager.hpp"

#include <algorithm>

using eng::InputManager;

constexpr int ANY_INPUT = -996;

void InputManager::handleKey(const int key, const int scancode, const bool down, const int mods)
{
    updateStateBoolean(keyMap, scancode, down);
    updateStateBoolean(keyMap, ANY_INPUT, down);
}

void InputManager::handleMouseButton(const int button, const bool down)
{
    updateStateBoolean(mouseButtonMap, button, down);
    updateStateBoolean(mouseButtonMap, ANY_INPUT, down);
}

void InputManager::handleMouseMotion(const float x, const float y, const float dx, const float dy)
{
    updateStateReal(cursorMap, static_cast<int>(CursorAxis::X), x);
    updateStateReal(cursorMap, static_cast<int>(CursorAxis::Y), y);
    updateStateDeltaReal(cursorMap, static_cast<int>(CursorAxis::X), dx);
    updateStateDeltaReal(cursorMap, static_cast<int>(CursorAxis::Y), dy);
}

void InputManager::handleGamepadConnection(const int gamepad, const bool connected)
{
    auto it = std::find(gamepads.begin(), gamepads.end(), gamepad);
    if (connected && it == gamepads.end())
    {
        gamepads.push_back(gamepad);
    }
    else if (!connected && it != gamepads.end())
    {
        gamepads.erase(it);
    }
}

void InputManager::handleGamepadAxisMotion(const int gamepad, const int axis, const float value)
{
    // TODO: fix this if we need to support multiple connected gamepads
    updateStateReal(gamepadAxisMap, axis, value);
}

void InputManager::handleGamepadButton(const int gamepad, const int button, const bool down)
{
    // TODO: fix this if we need to support multiple connected gamepads
    updateStateBoolean(gamepadButtonMap, button, down);
    updateStateBoolean(gamepadButtonMap, ANY_INPUT, down);
}

InputManager::InputManager()
{
    inputs.push_back(Input {});
}

uint32_t InputManager::createMapping()
{
    uint32_t index = mappings.size();
    mappings.push_back(Mapping {
            .inputIndex = 0,
        });
    return index;
}

uint32_t InputManager::mapKey(const uint32_t mapping, const int scancode, const BoolStateEvent event)
{
    unmap(mapping);
    map(mapping, getInputIndex(keyMap, scancode, Input {
                .inputType = InputType::Key,
                .code = scancode,
            }));
    mappings[mapping].event.boolState = event;
    return mapping;
}

uint32_t InputManager::mapMouseButton(const uint32_t mapping, const int button, const BoolStateEvent event)
{
    unmap(mapping);
    map(mapping, getInputIndex(mouseButtonMap, button, Input {
                .inputType = InputType::MouseButton,
                .code = button,
            }));
    mappings[mapping].event.boolState = event;
    return mapping;
}

uint32_t InputManager::mapCursor(const uint32_t mapping, const CursorAxis axis, const RealStateEvent event, const float param)
{
    unmap(mapping);
    map(mapping, getInputIndex(cursorMap, static_cast<int>(axis), Input {
                .inputType = InputType::Cursor,
                .code = static_cast<int>(axis),
            }));
    mappings[mapping].event.realState = event;
    mappings[mapping].params[0] = param;
    return mapping;
}

uint32_t InputManager::mapGamepadAxis(const uint32_t mapping, const int axis, const RealStateEvent event, const float param)
{
    unmap(mapping);
    map(mapping, getInputIndex(gamepadAxisMap, axis, Input {
                .inputType = InputType::GamepadAxis,
                .code = axis,
            }));
    mappings[mapping].event.realState = event;
    mappings[mapping].params[0] = param;
    return mapping;
}

uint32_t InputManager::mapGamepadButton(const uint32_t mapping, const int button, const BoolStateEvent event)
{
    unmap(mapping);
    map(mapping, getInputIndex(gamepadButtonMap, button, Input {
                .inputType = InputType::GamepadButton,
                .code = button,
            }));
    mappings[mapping].event.boolState = event;
    return mapping;
}

uint32_t InputManager::mapAnyKey(const uint32_t mapping, const BoolStateEvent event)
{
    return mapKey(mapping, ANY_INPUT, event);
}

uint32_t InputManager::mapAnyMouseButton(const uint32_t mapping, const BoolStateEvent event)
{
    return mapMouseButton(mapping, ANY_INPUT, event);
}

uint32_t InputManager::mapAnyGamepadButton(const uint32_t mapping, const BoolStateEvent event)
{
    return mapGamepadButton(mapping, ANY_INPUT, event);
}

bool InputManager::getBoolean(const uint32_t mapping) const
{
    const auto& input = inputs[mappings[mapping].inputIndex];
    bool result = false;
    switch (getInputTypeStateType(input.inputType))
    {
        case StateType::Boolean:
            switch (mappings[mapping].event.boolState)
            {
                case BoolStateEvent::Down:
                    result = input.state.boolean;
                    break;
                case BoolStateEvent::Pressed:
                    result = input.state.boolean && !input.previousState.boolean;
                    break;
                case BoolStateEvent::Released:
                    result = !input.state.boolean && input.previousState.boolean;
                    break;
                default:
                    break;
            }
            break;
        case StateType::Real:
            if (mappings[mapping].event.realState == RealStateEvent::Threshold)
            {
                result = input.state.real > mappings[mapping].params[0];
            }
            else
            {
                result = static_cast<bool>(getReal(mapping));
            }
            break;
        default:
            break;
    }
    return result;
}

double InputManager::getReal(const uint32_t mapping) const
{
    const auto& input = inputs[mappings[mapping].inputIndex];
    double result = 0.0;
    switch (getInputTypeStateType(input.inputType))
    {
        case StateType::Boolean:
            result = static_cast<double>(getBoolean(mapping));
            break;
        case StateType::Real:
            switch (mappings[mapping].event.realState)
            {
                case RealStateEvent::Value:
                    result = input.state.real;
                    break;
                case RealStateEvent::Delta:
                    if (!input.initial) result = input.state.real - input.previousState.real;
                    break;
                case RealStateEvent::Threshold:
                    result = static_cast<double>(input.state.real > mappings[mapping].params[0]);
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
    return result;
}

void InputManager::nextFrame()
{
    for (auto& input : inputs)
    {
        input.previousState = input.state;
        input.initial = false;
    }
}

void InputManager::map(const uint32_t mapping, const uint32_t inputIndex)
{
    mappings[mapping].inputIndex = inputIndex;
    ++inputs[inputIndex].mappingCount;
}

void InputManager::unmap(const uint32_t mapping)
{
    uint32_t inputIndex = mappings[mapping].inputIndex;
    if (inputIndex > 0 && (--inputs[inputIndex].mappingCount) == 0)
    {
        switch (inputs[inputIndex].inputType)
        {
        case InputType::None:
            break;
        case InputType::Key:
            keyMap.erase(inputs[inputIndex].code);
            break;
        case InputType::MouseButton:
            mouseButtonMap.erase(inputs[inputIndex].code);
            break;
        case InputType::Cursor:
            cursorMap.erase(inputs[inputIndex].code);
            break;
        case InputType::GamepadButton:
            gamepadButtonMap.erase(inputs[inputIndex].code);
            break;
        case InputType::GamepadAxis:
            gamepadAxisMap.erase(inputs[inputIndex].code);
            break;
        }
        freeInputs.push_back(inputIndex);
    }
}

uint32_t InputManager::getInputIndex(std::map<int, uint32_t>& map, const int code, Input&& input)
{
    auto it = map.find(code);
    if (it == map.end())
    {
        bool inserted;
        if (freeInputs.empty())
        {
            std::tie(it, inserted) = map.emplace(code, inputs.size());
            inputs.push_back(std::forward<Input>(input));
        }
        else
        {
            uint32_t index = freeInputs.back();
            freeInputs.pop_back();
            std::tie(it, inserted) = map.emplace(code, index);
            inputs[index] = std::forward<Input>(input);
        }
    }
    return it->second;
}

void InputManager::updateStateBoolean(const std::map<int, uint32_t>& map, const int code, const bool state)
{
    if (auto it = map.find(code); it != map.end())
    {
        inputs[it->second].state.boolean = state;
    }
}

void InputManager::updateStateReal(const std::map<int, uint32_t>& map, const int code, const double state)
{
    if (auto it = map.find(code); it != map.end())
    {
        inputs[it->second].state.real = state;
    }
}

void InputManager::updateStateDeltaReal(const std::map<int, uint32_t>& map, const int code, const double delta)
{
    if (auto it = map.find(code); it != map.end())
    {
        // this looks janky bc it is
        auto& inp = inputs[it->second];
        inp.previousState.real = inp.state.real - delta;
    }
}
