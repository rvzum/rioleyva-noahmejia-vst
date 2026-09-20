Implemented ahead of the original Phase 8 slot, at the user's request.

ThemeColors.h holds every color constant used by PluginEditor,
Visualization, and Effects. Restyling the plugin means editing (or later,
replacing with swappable presets in) this one file -- no other module
should ever hardcode a colour.
