workspace(name = "org_tensorflow")

# Uncomment and update the paths in these entries to build the Android demo.
#android_sdk_repository(
#    name = "androidsdk",
#    api_level = 23,
#    build_tools_version = "23.0.1",
#    # Replace with path to Android SDK on your system
#    path = "<PATH_TO_SDK>",
#)
#
#android_ndk_repository(
#    name="androidndk",
#    path="<PATH_TO_NDK>",
#    api_level=21)

# Please add all new TensorFlow dependencies in workspace.bzl.
load("//tensorflow:workspace.bzl", "tf_workspace")
tf_workspace()

# Specify the minimum required bazel version.
load("//tensorflow:tensorflow.bzl", "check_version")
check_version("0.3.2")

new_http_archive(
  name = "inception5h",
  build_file = "models.BUILD",
  url = "https://storage.googleapis.com/download.tensorflow.org/models/inception5h.zip",
  sha256 = "d13569f6a98159de37e92e9c8ec4dae8f674fbf475f69fe6199b514f756d4364"
)

new_http_archive(
  name = "mobile_multibox",
  build_file = "models.BUILD",
  url = "https://storage.googleapis.com/download.tensorflow.org/models/mobile_multibox_v1.zip",
  sha256 = "b4c178fd6236dcf0a20d25d07c45eebe85281263978c6a6f1dfc49d75befc45f"
)

# TENSORBOARD_BOWER_AUTOGENERATED_BELOW_THIS_LINE_DO_NOT_EDIT

new_http_archive(
  name = "d3",
  build_file = "bower.BUILD",
  url = "https://github.com/mbostock-bower/d3-bower/archive/v3.5.15.tar.gz",
  strip_prefix = "d3-bower-3.5.15",
)

new_http_archive(
  name = "dagre",
  build_file = "bower.BUILD",
  url = "https://github.com/cpettitt/dagre/archive/v0.7.4.tar.gz",
  strip_prefix = "dagre-0.7.4",
)

new_http_archive(
  name = "es6_promise",
  build_file = "bower.BUILD",
  url = "https://github.com/components/es6-promise/archive/v2.1.0.tar.gz",
  strip_prefix = "es6-promise-2.1.0",
)

new_http_archive(
  name = "font_roboto",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/font-roboto/archive/v1.0.1.tar.gz",
  strip_prefix = "font-roboto-1.0.1",
)

new_http_archive(
  name = "graphlib",
  build_file = "bower.BUILD",
  url = "https://github.com/cpettitt/graphlib/archive/v1.0.7.tar.gz",
  strip_prefix = "graphlib-1.0.7",
)

new_http_archive(
  name = "iron_a11y_announcer",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-a11y-announcer/archive/v1.0.5.tar.gz",
  strip_prefix = "iron-a11y-announcer-1.0.5",
)

new_http_archive(
  name = "iron_a11y_keys_behavior",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-a11y-keys-behavior/archive/v1.1.8.tar.gz",
  strip_prefix = "iron-a11y-keys-behavior-1.1.8",
)

new_http_archive(
  name = "iron_ajax",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-ajax/archive/v1.2.0.tar.gz",
  strip_prefix = "iron-ajax-1.2.0",
)

new_http_archive(
  name = "iron_autogrow_textarea",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-autogrow-textarea/archive/v1.0.12.tar.gz",
  strip_prefix = "iron-autogrow-textarea-1.0.12",
)

new_http_archive(
  name = "iron_behaviors",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-behaviors/archive/v1.0.17.tar.gz",
  strip_prefix = "iron-behaviors-1.0.17",
)

new_http_archive(
  name = "iron_checked_element_behavior",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-checked-element-behavior/archive/v1.0.4.tar.gz",
  strip_prefix = "iron-checked-element-behavior-1.0.4",
)

new_http_archive(
  name = "iron_collapse",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-collapse/archive/v1.0.8.tar.gz",
  strip_prefix = "iron-collapse-1.0.8",
)

new_http_archive(
  name = "iron_dropdown",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-dropdown/archive/v1.4.0.tar.gz",
  strip_prefix = "iron-dropdown-1.4.0",
)

new_http_archive(
  name = "iron_fit_behavior",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-fit-behavior/archive/v1.2.5.tar.gz",
  strip_prefix = "iron-fit-behavior-1.2.5",
)

new_http_archive(
  name = "iron_flex_layout",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-flex-layout/archive/v1.3.0.tar.gz",
  strip_prefix = "iron-flex-layout-1.3.0",
)

new_http_archive(
  name = "iron_form_element_behavior",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-form-element-behavior/archive/v1.0.6.tar.gz",
  strip_prefix = "iron-form-element-behavior-1.0.6",
)

new_http_archive(
  name = "iron_icon",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-icon/archive/v1.0.11.tar.gz",
  strip_prefix = "iron-icon-1.0.11",
)

new_http_archive(
  name = "iron_icons",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-icons/archive/v1.1.3.tar.gz",
  strip_prefix = "iron-icons-1.1.3",
)

new_http_archive(
  name = "iron_iconset_svg",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-iconset-svg/archive/v1.1.0.tar.gz",
  strip_prefix = "iron-iconset-svg-1.1.0",
)

new_http_archive(
  name = "iron_input",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-input/archive/1.0.10.tar.gz",
  strip_prefix = "iron-input-1.0.10",
)

new_http_archive(
  name = "iron_list",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-list/archive/v1.3.9.tar.gz",
  strip_prefix = "iron-list-1.3.9",
)

new_http_archive(
  name = "iron_menu_behavior",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-menu-behavior/archive/v1.1.10.tar.gz",
  strip_prefix = "iron-menu-behavior-1.1.10",
)

new_http_archive(
  name = "iron_meta",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-meta/archive/v1.1.1.tar.gz",
  strip_prefix = "iron-meta-1.1.1",
)

new_http_archive(
  name = "iron_overlay_behavior",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-overlay-behavior/archive/v1.10.1.tar.gz",
  strip_prefix = "iron-overlay-behavior-1.10.1",
)

new_http_archive(
  name = "iron_range_behavior",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-range-behavior/archive/v1.0.4.tar.gz",
  strip_prefix = "iron-range-behavior-1.0.4",
)

new_http_archive(
  name = "iron_resizable_behavior",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-resizable-behavior/archive/v1.0.3.tar.gz",
  strip_prefix = "iron-resizable-behavior-1.0.3",
)

new_http_archive(
  name = "iron_scroll_target_behavior",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-scroll-target-behavior/archive/v1.0.3.tar.gz",
  strip_prefix = "iron-scroll-target-behavior-1.0.3",
)

new_http_archive(
  name = "iron_selector",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-selector/archive/v1.5.2.tar.gz",
  strip_prefix = "iron-selector-1.5.2",
)

new_http_archive(
  name = "iron_validatable_behavior",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/iron-validatable-behavior/archive/v1.1.1.tar.gz",
  strip_prefix = "iron-validatable-behavior-1.1.1",
)

new_http_archive(
  name = "lodash",
  build_file = "bower.BUILD",
  url = "https://github.com/lodash/lodash/archive/3.8.0.tar.gz",
  strip_prefix = "lodash-3.8.0",
)

new_http_archive(
  name = "neon_animation",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/neon-animation/archive/v1.2.2.tar.gz",
  strip_prefix = "neon-animation-1.2.2",
)

http_file(
  name = "numericjs_numeric_min_js",
  url = "https://cdnjs.cloudflare.com/ajax/libs/numeric/1.2.6/numeric.min.js",
)

new_http_archive(
  name = "paper_behaviors",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-behaviors/archive/v1.0.12.tar.gz",
  strip_prefix = "paper-behaviors-1.0.12",
)

new_http_archive(
  name = "paper_button",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-button/archive/v1.0.11.tar.gz",
  strip_prefix = "paper-button-1.0.11",
)

new_http_archive(
  name = "paper_checkbox",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-checkbox/archive/v1.4.0.tar.gz",
  strip_prefix = "paper-checkbox-1.4.0",
)

new_http_archive(
  name = "paper_dialog",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-dialog/archive/v1.0.4.tar.gz",
  strip_prefix = "paper-dialog-1.0.4",
)

new_http_archive(
  name = "paper_dialog_behavior",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-dialog-behavior/archive/v1.2.5.tar.gz",
  strip_prefix = "paper-dialog-behavior-1.2.5",
)

new_http_archive(
  name = "paper_dialog_scrollable",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-dialog-scrollable/archive/1.1.5.tar.gz",
  strip_prefix = "paper-dialog-scrollable-1.1.5",
)

new_http_archive(
  name = "paper_dropdown_menu",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-dropdown-menu/archive/v1.4.0.tar.gz",
  strip_prefix = "paper-dropdown-menu-1.4.0",
)

new_http_archive(
  name = "paper_header_panel",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-header-panel/archive/v1.1.4.tar.gz",
  strip_prefix = "paper-header-panel-1.1.4",
)

new_http_archive(
  name = "paper_icon_button",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-icon-button/archive/v1.1.3.tar.gz",
  strip_prefix = "paper-icon-button-1.1.3",
)

new_http_archive(
  name = "paper_input",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-input/archive/v1.1.18.tar.gz",
  strip_prefix = "paper-input-1.1.18",
)

new_http_archive(
  name = "paper_item",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-item/archive/v1.1.4.tar.gz",
  strip_prefix = "paper-item-1.1.4",
)

new_http_archive(
  name = "paper_listbox",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-listbox/archive/v1.1.2.tar.gz",
  strip_prefix = "paper-listbox-1.1.2",
)

new_http_archive(
  name = "paper_material",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-material/archive/v1.0.6.tar.gz",
  strip_prefix = "paper-material-1.0.6",
)

new_http_archive(
  name = "paper_menu",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-menu/archive/v1.2.2.tar.gz",
  strip_prefix = "paper-menu-1.2.2",
)

new_http_archive(
  name = "paper_menu_button",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-menu-button/archive/v1.5.1.tar.gz",
  strip_prefix = "paper-menu-button-1.5.1",
)

new_http_archive(
  name = "paper_progress",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-progress/archive/v1.0.9.tar.gz",
  strip_prefix = "paper-progress-1.0.9",
)

new_http_archive(
  name = "paper_radio_button",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-radio-button/archive/v1.1.2.tar.gz",
  strip_prefix = "paper-radio-button-1.1.2",
)

new_http_archive(
  name = "paper_radio_group",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-radio-group/archive/v1.0.9.tar.gz",
  strip_prefix = "paper-radio-group-1.0.9",
)

new_http_archive(
  name = "paper_ripple",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-ripple/archive/v1.0.5.tar.gz",
  strip_prefix = "paper-ripple-1.0.5",
)

new_http_archive(
  name = "paper_slider",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-slider/archive/v1.0.10.tar.gz",
  strip_prefix = "paper-slider-1.0.10",
)

new_http_archive(
  name = "paper_spinner",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-spinner/archive/v1.1.1.tar.gz",
  strip_prefix = "paper-spinner-1.1.1",
)

new_http_archive(
  name = "paper_styles",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-styles/archive/v1.1.4.tar.gz",
  strip_prefix = "paper-styles-1.1.4",
)

new_http_archive(
  name = "paper_tabs",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-tabs/archive/v1.7.0.tar.gz",
  strip_prefix = "paper-tabs-1.7.0",
)

new_http_archive(
  name = "paper_toast",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-toast/archive/v1.3.0.tar.gz",
  strip_prefix = "paper-toast-1.3.0",
)

new_http_archive(
  name = "paper_toggle_button",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-toggle-button/archive/v1.2.0.tar.gz",
  strip_prefix = "paper-toggle-button-1.2.0",
)

new_http_archive(
  name = "paper_toolbar",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-toolbar/archive/v1.1.4.tar.gz",
  strip_prefix = "paper-toolbar-1.1.4",
)

new_http_archive(
  name = "paper_tooltip",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerelements/paper-tooltip/archive/v1.1.2.tar.gz",
  strip_prefix = "paper-tooltip-1.1.2",
)

new_http_archive(
  name = "plottable",
  build_file = "bower.BUILD",
  url = "https://github.com/palantir/plottable/archive/v1.16.1.tar.gz",
  strip_prefix = "plottable-1.16.1",
)

new_http_archive(
  name = "polymer",
  build_file = "bower.BUILD",
  url = "https://github.com/polymer/polymer/archive/v1.7.0.tar.gz",
  strip_prefix = "polymer-1.7.0",
)

new_http_archive(
  name = "promise_polyfill",
  build_file = "bower.BUILD",
  url = "https://github.com/polymerlabs/promise-polyfill/archive/v1.0.0.tar.gz",
  strip_prefix = "promise-polyfill-1.0.0",
)

http_file(
  name = "three_js_three_min_js",
  url = "https://raw.githubusercontent.com/mrdoob/three.js/r77/build/three.min.js",
)

http_file(
  name = "three_js_orbitcontrols_js",
  url = "https://raw.githubusercontent.com/mrdoob/three.js/r77/examples/js/controls/OrbitControls.js",
)

new_http_archive(
  name = "web_animations_js",
  build_file = "bower.BUILD",
  url = "https://github.com/web-animations/web-animations-js/archive/2.2.1.tar.gz",
  strip_prefix = "web-animations-js-2.2.1",
)

new_http_archive(
  name = "webcomponentsjs",
  build_file = "bower.BUILD",
  url = "https://github.com/webcomponents/webcomponentsjs/archive/v0.7.22.tar.gz",
  strip_prefix = "webcomponentsjs-0.7.22",
)

http_file(
  name = "weblas_weblas_js",
  url = "https://raw.githubusercontent.com/waylonflinn/weblas/v0.9.0/dist/weblas.js",
)
