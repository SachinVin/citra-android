package org.citra.citra_emu.features.settings.model;

import android.text.TextUtils;

import org.citra.citra_emu.features.settings.ui.SettingsActivityView;
import org.citra.citra_emu.features.settings.utils.SettingsFile;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

public class Settings {
    public static final String SECTION_CORE = "Core";
    public static final String SECTION_SYSTEM = "System";
    public static final String SECTION_CONTROLS = "Controls";
    public static final String SECTION_RENDERER = "Renderer";
    public static final String SECTION_AUDIO = "Audio";

    private String gameId;

    private static final Map<String, List<String>> configFileSectionsMap = new HashMap<>();

    static {
        configFileSectionsMap.put(SettingsFile.FILE_NAME_CONFIG, Arrays.asList(SECTION_CORE, SECTION_SYSTEM, SECTION_CONTROLS, SECTION_RENDERER, SECTION_AUDIO));
    }

    /**
     * A HashMap<String, SettingSection> that constructs a new SettingSection instead of returning null
     * when getting a key not already in the map
     */
    public static final class SettingsSectionMap extends HashMap<String, SettingSection> {
        @Override
        public SettingSection get(Object key) {
            if (!(key instanceof String)) {
                return null;
            }

            String stringKey = (String) key;

            if (!super.containsKey(stringKey)) {
                SettingSection section = new SettingSection(stringKey);
                super.put(stringKey, section);
                return section;
            }
            return super.get(key);
        }
    }

    private HashMap<String, SettingSection> sections = new Settings.SettingsSectionMap();

    public SettingSection getSection(String sectionName) {
        return sections.get(sectionName);
    }

    public boolean isEmpty() {
        return sections.isEmpty();
    }

    public HashMap<String, SettingSection> getSections() {
        return sections;
    }

    public void loadSettings(SettingsActivityView view) {
        sections = new Settings.SettingsSectionMap();
        if (TextUtils.isEmpty(gameId)) {
            for (Map.Entry<String, List<String>> entry : configFileSectionsMap.entrySet()) {
                String fileName = entry.getKey();
                sections.putAll(SettingsFile.readFile(fileName, view));
            }
        } else {
            // custom game settings
            sections.putAll(SettingsFile.readFile("../GameSettings/" + gameId, view));
        }
    }

    public void loadSettings(String gameId, SettingsActivityView view) {
        this.gameId = gameId;
        loadSettings(view);
    }

    public void saveSettings(SettingsActivityView view) {
        if (TextUtils.isEmpty(gameId)) {
            view.showToastMessage("Saved settings to INI files", false);

            for (Map.Entry<String, List<String>> entry : configFileSectionsMap.entrySet()) {
                String fileName = entry.getKey();
                List<String> sectionNames = entry.getValue();
                TreeMap<String, SettingSection> iniSections = new TreeMap<>();
                for (String section : sectionNames) {
                    iniSections.put(section, sections.get(section));
                }

                SettingsFile.saveFile(fileName, iniSections, view);
            }
        } else {
            // custom game settings
            view.showToastMessage("Saved settings for " + gameId, false);

            TreeMap<String, SettingSection> iniSections = new TreeMap<>(sections);
            SettingsFile.saveFile("../GameSettings/" + gameId, iniSections, view);
        }

    }
}