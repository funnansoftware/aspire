package org.aspire.evford;

import org.libsdl.app.SDLActivity;

public final class EvfordActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }
}
