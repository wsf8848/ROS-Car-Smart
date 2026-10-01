import java.util.Properties
import java.io.FileInputStream
import java.io.File

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

// 读取签名配置（keystore/release-signing.properties）
val signingPropsFile = rootProject.file("keystore/release-signing.properties")
val signingProps = Properties().apply {
    if (signingPropsFile.exists()) load(FileInputStream(signingPropsFile))
}

// MQTT 凭据只从工程外层受控 env 注入；目录联接构建时先解析真实工程路径。
val mqttEnvCandidates = mutableListOf<File>()
System.getenv("BEEBOT_MQTT_ENV")
    ?.takeIf { it.isNotBlank() }
    ?.let { mqttEnvCandidates += File(it) }
listOfNotNull(
    runCatching { rootProject.rootDir.toPath().toRealPath().toFile() }.getOrNull(),
    rootProject.rootDir
).distinctBy { it.absolutePath.lowercase() }.forEach { projectRoot ->
    projectRoot.parentFile?.parentFile
        ?.resolve("1-0 服务器接入说明/mqtt-connection.env")
        ?.let { mqttEnvCandidates += it }
}
val mqttEnvFile = mqttEnvCandidates.firstOrNull { it.isFile }
val mqttEnv = mutableMapOf<String, String>()
if (mqttEnvFile?.exists() == true) {
    mqttEnvFile.readLines(Charsets.UTF_8).forEach { raw ->
        val line = raw.trim()
        if (line.isNotEmpty() && !line.startsWith("#") && line.contains('=')) {
            val (key, value) = line.split('=', limit = 2)
            mqttEnv[key.trim()] = value.trim().trim('"', '\'')
        }
    }
}
// 萤石配置与 MQTT 凭据分离；AppSecret 只保存在服务器，不允许注入 APK。
val ezvizEnvCandidates = mutableListOf<File>()
System.getenv("BEEBOT_EZVIZ_ENV")
    ?.takeIf { it.isNotBlank() }
    ?.let { ezvizEnvCandidates += File(it) }
listOfNotNull(
    runCatching { rootProject.rootDir.toPath().toRealPath().toFile() }.getOrNull(),
    rootProject.rootDir
).distinctBy { it.absolutePath.lowercase() }.forEach { projectRoot ->
    projectRoot.parentFile?.parentFile
        ?.resolve("1-0 服务器接入说明/ezviz-connection.env")
        ?.let { ezvizEnvCandidates += it }
}
val ezvizEnv = mutableMapOf<String, String>()
ezvizEnvCandidates.firstOrNull { it.isFile }?.readLines(Charsets.UTF_8)?.forEach { raw ->
    val line = raw.trim()
    if (line.isNotEmpty() && !line.startsWith("#") && line.contains('=')) {
        val (key, value) = line.split('=', limit = 2)
        ezvizEnv[key.trim()] = value.trim().trim('"', '\'')
    }
}
fun buildConfigString(key: String, fallback: String): String {
    val value = (mqttEnv[key] ?: fallback).replace("\\", "\\\\").replace("\"", "\\\"")
    return "\"$value\""
}

fun buildEzvizConfigString(key: String, fallback: String = ""): String {
    val value = (ezvizEnv[key] ?: fallback).replace("\\", "\\\\").replace("\"", "\\\"")
    return "\"$value\""
}

android {
    namespace = "com.fyx.beebot"
    compileSdk = 34
    buildToolsVersion = "34.0.0"

    defaultConfig {
        applicationId = "com.fyx.beebot"
        minSdk = 24
        targetSdk = 34
        versionCode = 37
        versionName = "2.6.3"
        ndk {
            abiFilters += "arm64-v8a"
        }
        vectorDrawables { useSupportLibrary = true }
        buildConfigField("String", "MQTT_WSS_URL", buildConfigString("MQTT_WSS_URL", ""))
        buildConfigField("String", "MQTT_USERNAME", buildConfigString("CONTROLLER_USERNAME", ""))
        buildConfigField("String", "MQTT_PASSWORD", buildConfigString("CONTROLLER_PASSWORD", ""))
        buildConfigField("String", "MQTT_CONTROL_TOPIC", buildConfigString("CONTROL_TOPIC", "car/001/control"))
        buildConfigField("String", "MQTT_TELEMETRY_TOPIC", buildConfigString("TELEMETRY_TOPIC", "car/001/telemetry"))
        buildConfigField("String", "MQTT_STATUS_TOPIC", buildConfigString("STATUS_TOPIC", "car/001/status"))
        buildConfigField("String", "EZVIZ_APP_KEY", buildEzvizConfigString("EZVIZ_APP_KEY"))
        buildConfigField("String", "EZVIZ_ACCESS_TOKEN", buildEzvizConfigString("EZVIZ_ACCESS_TOKEN"))
        buildConfigField("String", "EZVIZ_DEVICE_SERIAL", buildEzvizConfigString("EZVIZ_DEVICE_SERIAL"))
        buildConfigField("String", "EZVIZ_CHANNEL_NO", buildEzvizConfigString("EZVIZ_CHANNEL_NO", "1"))
        buildConfigField("String", "EZVIZ_VERIFY_CODE", buildEzvizConfigString("EZVIZ_VERIFY_CODE"))
    }

    signingConfigs {
        if (signingPropsFile.exists()) {
            create("release") {
                storeFile = rootProject.file(signingProps.getProperty("storeFile"))
                storePassword = signingProps.getProperty("storePassword")
                keyAlias = signingProps.getProperty("keyAlias")
                keyPassword = signingProps.getProperty("keyPassword")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            if (signingPropsFile.exists()) {
                signingConfig = signingConfigs.getByName("release")
            }
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
        buildConfig = true
    }
    composeOptions {
        // Kotlin 1.8.20 对应 Compose Compiler 1.4.6（本机硬约束）
        kotlinCompilerExtensionVersion = "1.4.6"
    }
    packaging {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
    }
    androidResources {
        noCompress += "onnx"
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.10.1")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.6.1")
    implementation("androidx.activity:activity-compose:1.7.2")

    implementation("androidx.compose.ui:ui:1.4.3")
    implementation("androidx.compose.ui:ui-graphics:1.4.3")
    implementation("androidx.compose.ui:ui-tooling-preview:1.4.3")
    implementation("androidx.compose.material3:material3:1.1.0")
    implementation("androidx.compose.material:material-icons-extended:1.4.3")
    implementation("org.eclipse.paho:org.eclipse.paho.client.mqttv3:1.2.5")
    // 官方 v5.32 二进制固定到本地，规避本机构建时 Maven Central TLS 握手不稳定。
    implementation(files("libs/ezviz-sdk-5.32.aar"))
    implementation(files("libs/okhttp-3.12.1.jar"))
    implementation(files("libs/okio-1.15.0.jar"))
    implementation(files("libs/gson-2.8.5.jar"))
    implementation(files("libs/sherpa-onnx-1.13.8.aar"))

    testImplementation("junit:junit:4.13.2")

    debugImplementation("androidx.compose.ui:ui-tooling:1.4.3")
}
