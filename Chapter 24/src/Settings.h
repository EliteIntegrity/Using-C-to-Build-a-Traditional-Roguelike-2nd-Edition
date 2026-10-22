#pragma once

// The player's settings, kept in settings.txt in their own folder, so they
// last from one run to the next, and from one day to the next
struct Settings
{
    bool shake = true;   // the screen shakes on big hits and blasts
};

// The settings saved in settings.txt, or, if there's no such file yet, the
// ones above. A mistake in the file is logged, and the game goes on with
// what it could read: settings aren't worth stopping the game for
Settings loadSettings();

// Writes the settings to settings.txt. Returns false if it couldn't
bool saveSettings(const Settings& settings);
