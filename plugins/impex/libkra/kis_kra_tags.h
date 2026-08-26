/* This file is part of the KDE project
 * Copyright 2008 (C) Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KIS_KRA_TAGS
#define KIS_KRA_TAGS

#include <PkString.h>
#include <KisResourceTypes.h>

/**
 * Tag definitions for our xml file format
 */
namespace KRA
{

// mimetype
const PkString NATIVE_MIMETYPE = "application/x-kra";

// xml tags
const PkString SEPARATOR = "/";
const PkString SHAPE_LAYER_PATH = "/shapelayers/";
const PkString EXIF_PATH = "/annotations/exif";
const PkString ANNOTATIONS_PATH = "/annotations/";
const PkString ICC_PATH = "/annotations/icc";
const PkString ICC_PROOFING_PATH = "/annotations/proofing/icc";
const PkString LAYER_STYLES_PATH = "/annotations/layerstyles.asl";
const PkString ASSISTANTS_PATH = "/assistants/";
const PkString LAYER_PATH = "/layers/";
const PkString PALETTE_PATH = "/palettes/";
const PkString RESOURCE_PATH = "resources/"; // Note: intentionally not slash in front.
const PkString STORYBOARD_PATH = "/storyboard/";
const PkString AUDIO_PATH = "/audio/";
const PkString ANIMATION_METADATA_PATH = "/animation/";

const PkString ADJUSTMENT_LAYER = "adjustmentlayer";
const PkString CHANNEL_FLAGS = "channelflags";
const PkString CHANNEL_LOCK_FLAGS = "channellockflags";
const PkString CLONE_FROM = "clonefrom";
const PkString CLONE_FROM_UUID = "clonefromuuid";
const PkString CLONE_LAYER = "clonelayer";
const PkString CLONE_TYPE = "clonetype";
const PkString COLORSPACE_NAME = "colorspacename";
const PkString COMPOSITE_OP = "compositeop";
const PkString DESCRIPTION = "description";
const PkString ONION_SKIN_ENABLED = "onionskin";
const PkString VISIBLE_IN_TIMELINE = "intimeline";

const PkString DOT_FILTERCONFIG = ".filterconfig";
const PkString DOT_TRANSFORMCONFIG = ".transformconfig";
const PkString DOT_ICC = ".icc";
const PkString DOT_PIXEL_SELECTION = ".pixelselection";
const PkString DOT_SHAPE_SELECTION = ".shapeselection";
const PkString DOT_SHAPE_LAYER = ".shapelayer";
const PkString DOT_COLORIZE_MASK = ".colorizemask";
const PkString DOT_METADATA = ".metadata";

const PkString FILE_NAME = "filename";
const PkString FILTER_MASK = "filtermask";
const PkString FILTER_NAME = "filtername";
const PkString FILTER_STRATEGY = "filter_strategy";
const PkString FILTER_VERSION = "filterversion";
const PkString GENERATOR_LAYER = "generatorlayer";
const PkString GENERATOR_NAME = "generatorname";
const PkString GENERATOR_VERSION = "generatorversion";
const PkString GROUP_LAYER = "grouplayer";
const PkString HEIGHT = "height";
const PkString ICC = "icc";
const PkString LAYER = "layer";
const PkString LAYERS = "layers";
const PkString NODE_TYPE = "nodetype";
const PkString LOCKED = "locked";
const PkString ANTIALIASED = "antialiased";
const PkString MASK = "mask";
const PkString MASKS = "masks";
const PkString MIME = "mime";
const PkString NAME = "name";
const PkString OPACITY = "opacity";
const PkString COLLAPSED = "collapsed";
const PkString COLOR_LABEL = "colorlabel";
const PkString PAINT_LAYER = "paintlayer";
const PkString PROFILE = "profile";
const PkString ROTATION = "rotation";
const PkString SELECTION_MASK = "selectionmask";
const PkString SHAPE_LAYER = "shapelayer";
const PkString REFERENCE_IMAGES_LAYER = "referenceimages";
const PkString FILE_LAYER = "filelayer";
const PkString TRANSPARENCY_MASK = "transparencymask";
const PkString COLORIZE_MASK = "colorizemask";
const PkString COLORIZE_SHOW_COLORING = "show-coloring";
const PkString COLORIZE_EDIT_KEYSTROKES = "edit-keystrokes";
const PkString COLORIZE_KEYSTROKE = "keystroke";
const PkString COLORIZE_KEYSTROKE_COLOR = "color";
const PkString COLORIZE_KEYSTROKE_IS_TRANSPARENT = "is-transparent";
const PkString COLORIZE_COLORING_DEVICE = "colorize-coloring";
const PkString COLORIZE_KEYSTROKES_SECTION = "keystrokes";
const PkString COLORIZE_USE_EDGE_DETECTION = "use-edge-detection";
const PkString COLORIZE_EDGE_DETECTION_SIZE = "edge-detection-size";
const PkString COLORIZE_FUZZY_RADIUS = "fuzzy-radius";
const PkString COLORIZE_CLEANUP = "cleanup";
const PkString COLORIZE_LIMIT_TO_DEVICE = "limit-to-device";
const PkString TRANSFORM_MASK = "transformmask";
const PkString UUID = "uuid";
const PkString VISIBLE = "visible";
const PkString WIDTH = "width";
const PkString X = "x";
const PkString X_RESOLUTION = "x-res";
const PkString X_SCALE = "x_scale";
const PkString X_SHEAR = "x_shear";
const PkString X_TRANSLATION = "x_translation";
const PkString Y = "y";
const PkString Y_RESOLUTION = "y-res";
const PkString Y_SCALE = "y_scale";
const PkString Y_SHEAR = "y_shear";
const PkString Y_TRANSLATION = "y_translation";
const PkString ACTIVE = "active";
const PkString LAYER_STYLE_UUID = "layerstyle";
const PkString PASS_THROUGH_MODE = "passthrough";
const PkString KEYFRAME_FILE = "keyframes";
const PkString PROOFINGPROFILENAME = "proofing-profile-name";
const PkString PROOFINGMODEL = "proofing-model";
const PkString PROOFINGDEPTH = "proofing-depth";
const PkString PROOFINGINTENT = "proofing-intent";
const PkString PROOFINGDISPLAYINTENT = "proofing-display-intent";
const PkString PROOFINGBLACKPOINTCOMPENSATION = "proofing-blackpoint-compensation";
const PkString PROOFINGDISPLAYBLACKPOINTCOMPENSATION = "proofing-display-blackpoint-compensation";
const PkString PROOFINGDISPLAYMODE = "proofing-display-mode";
const PkString PROOFINGWARNINGCOLOR ="ProofingWarningColor";
const PkString PROOFINGADAPTATIONSTATE = "proofing-adaptation-state";
const PkString ICCPROOFINGPROFILE ="icc-proofing-profile";
const PkString CANVASPROJECTIONCOLOR = "ProjectionBackgroundColor";
const PkString COLORBYTEDATA = "ColorData";
const PkString COLORHISTORY = "ColorHistory";
const PkString COLORLIST = "ColorList";
const PkString SIMPLECOLORDATA = "SimpleColorData"; // easier 8-bit color data that works well with XML
const PkString GLOBALASSISTANTSCOLOR = "GlobalAssistantsColor";
const PkString PALETTES = "Palettes"; // ResourceType::Palettes is lowercase, while the tag is uppercase
const PkString RESOURCES = "resources";
const PkString MIRROR_AXIS = "MirrorAxis";
const PkString ANNOTATIONS = "Annotations";
const PkString ANNOTATION = "Annotation";
}



#endif
