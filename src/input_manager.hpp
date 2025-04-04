#include "engine.hpp"

#include <map>

struct GLFWwindow;

namespace eng
{
    class InputManager : public InputInterface
    {
        enum class InputType
        {
            None,
            Key,
            MouseButton,
            Cursor,
            GamepadButton,
            GamepadAxis,
        };

        enum class StateType
        {
            Boolean, Real,
        };

        static constexpr StateType getInputTypeStateType(const InputType inputType)
        {
            switch (inputType)
            {
                case InputType::None:
                    return StateType::Boolean;
                case InputType::Key:
                    return StateType::Boolean;
                case InputType::MouseButton:
                    return StateType::Boolean;
                case InputType::Cursor:
                    return StateType::Real;
                case InputType::GamepadButton:
                    return StateType::Boolean;
                case InputType::GamepadAxis:
                    return StateType::Real;
                default:
                    break;
            }
            return StateType::Boolean;
        }

        struct Input
        {
            InputType inputType = InputType::None;
            int code;

            union
            {
                bool boolean;
                double real;
            } state, previousState;
            bool initial = true;

            int mappingCount = 0;
        };

        struct Mapping
        {
            uint32_t inputIndex;
            union {
                RealStateEvent realState;
                BoolStateEvent boolState;
            } event;

            float params[1] = { 0 };
        };

    public:
        explicit InputManager();

        uint32_t createMapping() override;

        uint32_t mapKey(const uint32_t mapping, const int scancode, const BoolStateEvent event) override;
        uint32_t mapMouseButton(const uint32_t mapping, const int button, const BoolStateEvent event) override;
        uint32_t mapCursor(const uint32_t mapping, const CursorAxis axis, const RealStateEvent event, const float param) override;
        uint32_t mapGamepadAxis(const uint32_t mapping, const int axis, const RealStateEvent, const float param) override;
        uint32_t mapGamepadButton(const uint32_t mapping, const int button, const BoolStateEvent) override;

        bool getBoolean(const uint32_t mapping/* , const BoolStateEvent event */) const override;
        double getReal(const uint32_t mapping/* , const RealStateEvent event */) const override;
        /* bool getDown(const uint32_t mapping) const;
        bool getPressed(const uint32_t mapping) const;
        bool getReleased(const uint32_t mapping) const; */

        void nextFrame();

        void handleKey(const int key, const int scancode, const bool down, const int mods);
        void handleMouseButton(const int button, const bool down);
        void handleMouseMotion(const float x, const float y, const float dx, const float dy);
        void handleGamepadConnection(const int gamepad, const bool connection);
        void handleGamepadAxisMotion(const int gamepad, const int axis, const float value);
        void handleGamepadButton(const int gamepad, const int button, const bool down);

    private:
        void map(const uint32_t mapping, const uint32_t inputIndex);
        void unmap(const uint32_t mapping);
        uint32_t getInputIndex(std::map<int, uint32_t>& map, const int code, Input&& input);
        void updateStateBoolean(const std::map<int, uint32_t>& map, const int code, const bool state);
        void updateStateReal(const std::map<int, uint32_t>& map, const int code, const double state);

        std::vector<Input> inputs;
        std::vector<Mapping> mappings;
        std::map<int, uint32_t> keyMap;
        std::map<int, uint32_t> mouseButtonMap;
        std::map<int, uint32_t> cursorMap;
        std::map<int, uint32_t> gamepadButtonMap;
        std::map<int, uint32_t> gamepadAxisMap;
        std::vector<uint32_t> freeInputs;
        std::vector<int> gamepads;
    };
}
