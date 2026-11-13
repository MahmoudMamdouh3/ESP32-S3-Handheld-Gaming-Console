#include "pause_quit_policy.h"

int main() {
  PauseQuit::State state;

  if (PauseQuit::transition(state, PauseQuit::Input::Confirm, true) != PauseQuit::Action::Stay ||
      !state.confirmationArmed)
    return 1;
  if (PauseQuit::transition(state, PauseQuit::Input::Confirm, true) != PauseQuit::Action::AttemptSave ||
      state.confirmationArmed)
    return 2;

  // Fake SaveManager reports a failed write. The session must stay open.
  if (PauseQuit::transition(state, PauseQuit::Input::SaveFailed, true) != PauseQuit::Action::Stay || !state.saveFailed)
    return 3;
  if (PauseQuit::transition(state, PauseQuit::Input::Confirm, true) != PauseQuit::Action::AttemptSave) return 4;
  if (PauseQuit::transition(state, PauseQuit::Input::SaveFailed, true) != PauseQuit::Action::Stay) return 5;

  // After retry also fails, an explicit Start/quit-anyway choice is required.
  if (PauseQuit::transition(state, PauseQuit::Input::QuitAnyway, true) != PauseQuit::Action::Quit || state.saveFailed ||
      state.confirmationArmed)
    return 6;

  if (PauseQuit::transition(state, PauseQuit::Input::Confirm, false) != PauseQuit::Action::Stay) return 7;
  if (PauseQuit::transition(state, PauseQuit::Input::Confirm, false) != PauseQuit::Action::Quit) return 8;

  if (PauseQuit::transition(state, PauseQuit::Input::Confirm, true) != PauseQuit::Action::Stay) return 9;
  if (PauseQuit::transition(state, PauseQuit::Input::Screenshot, true) != PauseQuit::Action::Screenshot ||
      state.confirmationArmed || state.saveFailed)
    return 10;

  if (PauseQuit::transition(state, PauseQuit::Input::Confirm, true) != PauseQuit::Action::Stay) return 11;
  if (PauseQuit::transition(state, PauseQuit::Input::Timeout, true) != PauseQuit::Action::Stay ||
      state.confirmationArmed)
    return 12;
  if (PauseQuit::transition(state, PauseQuit::Input::Resume, true) != PauseQuit::Action::Resume) return 13;
  return 0;
}
