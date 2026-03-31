#pragma once

// GWA3 Item/Bag/Inventory structs — matches GWCA GameEntities/Item.h memory layout.
// Cross-referenced with GWA2_Assembly.au3 $ITEM_STRUCT_TEMPLATE / $BAG_STRUCT_TEMPLATE.

#include <cstdint>
#include <gwa3/game/GameTypes.h>

namespace GWA3 {

struct DyeInfo { // total: 3 bytes
    uint8_t dye_tint;
    uint8_t dye_colors_1; // dye1:4 | dye2:4
    uint8_t dye_colors_2; // dye3:4 | dye4:4
};

struct ItemData { // total: 0x10 / 16 bytes
    /* +h0000 */ uint32_t model_file_id;
    /* +h0004 */ uint8_t  type;
    /* +h0005 */ DyeInfo  dye;
    /* +h0008 */ uint32_t value;
    /* +h000C */ uint32_t interaction;
};

struct ItemModifier { // total: 4 bytes
    uint32_t mod;
};

struct Item; // forward declaration

struct Bag { // total: 0x28 / 40 bytes
    /* +h0000 */ uint32_t        bag_type;
    /* +h0004 */ uint32_t        index;
    /* +h0008 */ uint32_t        h0008;
    /* +h000C */ uint32_t        container_item;
    /* +h0010 */ uint32_t        items_count;
    /* +h0014 */ Bag*            bag_array;
    /* +h0018 */ GWArray<Item*>  items;
};

struct Item { // total: 0x54 / 84 bytes
    /* +h0000 */ uint32_t      item_id;
    /* +h0004 */ uint32_t      agent_id;
    /* +h0008 */ Bag*          bag_equipped;
    /* +h000C */ Bag*          bag;
    /* +h0010 */ ItemModifier* mod_struct;
    /* +h0014 */ uint32_t      mod_struct_size;
    /* +h0018 */ wchar_t*      customized;
    /* +h001C */ uint32_t      model_file_id;
    /* +h0020 */ uint8_t       type;
    /* +h0021 */ DyeInfo       dye;
    /* +h0024 */ uint16_t      value;
    /* +h0026 */ uint16_t      h0026;
    /* +h0028 */ uint32_t      interaction;
    /* +h002C */ uint32_t      model_id;
    /* +h0030 */ wchar_t*      info_string;
    /* +h0034 */ wchar_t*      name_enc;
    /* +h0038 */ wchar_t*      complete_name_enc;
    /* +h003C */ wchar_t*      single_item_name;
    /* +h0040 */ uint32_t      h0040[2];
    /* +h0048 */ uint16_t      item_formula;
    /* +h004A */ uint8_t       is_material_salvageable;
    /* +h004B */ uint8_t       h004B;
    /* +h004C */ uint16_t      quantity;
    /* +h004E */ uint8_t       equipped;
    /* +h004F */ uint8_t       profession;
    /* +h0050 */ uint8_t       slot;
};

struct WeaponSet { // total: 0x08 / 8 bytes
    /* +h0000 */ Item* weapon;
    /* +h0004 */ Item* offhand;
};

struct Inventory { // total: 0x98 / 152 bytes
    /* +h0000 */ Bag*      bags[23];
    /* +h005C */ Item*     bundle;
    /* +h0060 */ uint32_t  storage_panes_unlocked;
    /* +h0064 */ WeaponSet weapon_sets[4];
    /* +h0084 */ uint32_t  active_weapon_set;
    /* +h0088 */ uint32_t  h0088[2];
    /* +h0090 */ uint32_t  gold_character;
    /* +h0094 */ uint32_t  gold_storage;
};

} // namespace GWA3
