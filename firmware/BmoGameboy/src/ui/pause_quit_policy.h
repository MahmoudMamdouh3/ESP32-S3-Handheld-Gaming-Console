#pragma once

namespace PauseQuit {

struct State {
  bool confirmationArmed = false;
  bool saveFailed = false;
};

enum class Input {
  Confirm,
  SaveSucceeded,
  SaveFailed,
  QuitAnyway,
  Resume,
  Screenshot,
  Cancel,
  Timeout,
};

enum class Action {
  Stay,
  AttemptSave,
  Quit,
  Resume,
  Screenshot,
};

inline Action transition(State& state, Input input, bool batterySaveSupported) {
  switch (input) {
  case Input::Confirm:
    if (state.saveFailed) return Action::AttemptSave;
    if (!state.confirmationArmed) {
      state.confirmationArmed = true;
      return Action::Stay;
    }
    state.confirmationArmed = false;
    return batterySaveSupported ? Action::AttemptSave : Action::Quit;

  case Input::SaveSucceeded:
    state = State{};
    return Action::Quit;

  case Input::SaveFailed:
    state.confirmationArmed = false;
    state.saveFailed = true;
    return Action::Stay;

  case Input::QuitAnyway:
    if (!state.saveFailed) return Action::Stay;
    state = State{};
    return Action::Quit;

  case Input::Resume:
    state = State{};
    return Action::Resume;

  case Input::Screenshot:
    state = State{};
    return Action::Screenshot;

  case Input::Cancel:
    state = State{};
    return Action::Stay;

  case Input::Timeout:
    state.confirmationArmed = false;
    return Action::Stay;
  }
  return Action::Stay;
}

} // namespace PauseQuit
