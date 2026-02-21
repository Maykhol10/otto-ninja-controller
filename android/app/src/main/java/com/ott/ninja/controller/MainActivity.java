package com.ott.ninja.controller;

import android.os.Bundle;
import android.view.View;
import androidx.core.splashscreen.SplashScreen;
import androidx.core.splashscreen.SplashScreen.Companion;
import com.getcapacitor.BridgeActivity;

public class MainActivity extends BridgeActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Install the splash screen before calling super.onCreate()
        SplashScreen.installSplashScreen(this);

        super.onCreate(savedInstanceState);
    }

    @Override
    public void onBackPressed() {
        // Prevent going back to splash screen
        moveTaskToBack(true);
    }
}
