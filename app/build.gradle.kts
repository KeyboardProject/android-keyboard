plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.jetbrains.kotlin.android)
}

android {
    namespace = "com.example.macro"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.example.macro"
        minSdk = 28
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        vectorDrawables {
            useSupportLibrary = true
        }

        ndk {
            abiFilters.apply {
                add("arm64-v8a")
                add("armeabi-v7a")
            }
        }
    }

    signingConfigs {
        create("release") {
            storeFile = (properties["RELEASE_STORE_FILE"] as? String ?: System.getenv("RELEASE_STORE_FILE")).let { file(it) }
            storePassword = properties["RELEASE_STORE_PASSWORD"] as? String ?: System.getenv("RELEASE_STORE_PASSWORD")
            keyAlias = properties["RELEASE_KEY_ALIAS"] as? String ?: System.getenv("RELEASE_KEY_ALIAS")
            keyPassword = properties["RELEASE_KEY_PASSWORD"] as? String ?: System.getenv("RELEASE_KEY_PASSWORD")
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            signingConfig = signingConfigs.getByName("release")
        }
        debug {
            isDebuggable = true
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
    buildFeatures {
        compose = true
    }
    composeOptions {
        kotlinCompilerExtensionVersion = "1.5.1"
    }

    ndkVersion = "26.1.10909125"
    buildToolsVersion = "35.0.0"
    packaging {
        resources {
            excludes += listOf(
                "/META-INF/{AL2.0,LGPL2.1}"
            )
        }
    }
}


val camerax_version = "1.3.4"

dependencies {

    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.ui)
    implementation(libs.androidx.ui.graphics)
    implementation(libs.androidx.ui.tooling.preview)
    implementation(libs.androidx.material3)
    implementation(project(":opencv"))
    implementation(libs.androidx.camera.lifecycle)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    androidTestImplementation(platform(libs.androidx.compose.bom))
    androidTestImplementation(libs.androidx.ui.test.junit4)
    debugImplementation(libs.androidx.ui.tooling)
    debugImplementation(libs.androidx.ui.test.manifest)
    implementation ("androidx.localbroadcastmanager:localbroadcastmanager:1.0.0")
}
