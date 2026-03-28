# GWCA Crafting / Consumables Vendor Research

## Goal
Investigate whether GWCA UI message mechanisms can be used to interact with consumables vendors (like Eyja [Consumables] in Embark Beach) for crafting items like Grail of Might, as an alternative to the current packet-based approach which has been unreliable.

---

## Current Crafting Architecture

### How It Works Now (Packet-Based)
The existing `CraftItemSafe()` in `Utils-Maintenance.au3` (lines 1061-1249) uses direct transaction packets:

1. **Opens vendor dialog** — walk to NPC, interact via Dialog()
2. **Finds item in merchant list** — walks `GetMerchantItemsBase()` array, matches by ModelID
3. **Validates materials** — checks inventory for required material stacks
4. **Allocates game-process memory** — two arrays: MatIDs and MatQtys via VirtualAllocEx
5. **Builds CRAFT_ITEM_STRUCT** (28 bytes, 7 fields):
   ```
   +0x00: CommandCraftItemEx2 address
   +0x04: AmountToCraft
   +0x08: MerchantItemPtr (destination item)
   +0x0C: TotalCost (amount × gold)
   +0x10: GiveCount (material stack count)
   +0x14: MatIDsArray ptr (game memory)
   +0x18: MatQtysArray ptr (game memory)
   ```
6. **Enqueues** via BotsHub command queue → `CommandCraftItemEx2` ASM
7. **CommandCraftItemEx2** calls `TransactionFunction` with opcode 3 (CrafterBuy)

### Known Issues
- Ecto buying logic disabled: `BuyEctosWithExcessGold()` commented out due to conflicts
- Debug test scripts exist (`Debug_Crafting_Values.au3`, `Sniff_Crafting_Packet.au3`, `Test_Craft_Grail.au3`) suggesting active troubleshooting
- The packet-based approach depends on:
  - Correct `TradeID` address initialization
  - Accurate merchant item pointer resolution (offset chain: `[base][0x18][0x40][0xB8][itemID*4]`)
  - Game-process memory allocation for material arrays
  - Proper `TransactionFunction` address from scan

### Consumable Recipes (from buyConsumablesInEmbarkBeach)
| Item | Vendor | Materials | Gold |
|---|---|---|---|
| Grail of Might | Eyja | 50 Iron Ingots + 50 Glittering Dust | 250 |
| Essence of Celerity | Kwat | 50 Feather + 50 Glittering Dust | 250 |
| Armor of Salvation | Alcus Nailbiter | 50 Iron Ingots + 50 Bone | 250 |
| Powerstone of Courage | Edwin | 100 Granite + 100 Glittering Dust | 1000 |
| Scroll of Resurrection | Edwin | 25 Plant Fiber + 25 Bone | 250 |

---

## Two Possible GWCA Approaches

### Approach A: UI Frame Click (Like Play Button)
Click the "Craft" button in the vendor dialog using GWCA's ButtonClick mechanism.

**Vendor Dialog UI Structure** (from screenshot):
- Window: "Eyja [Consumables]"
- Tab: "Craft" / "Sell"
- Item list (clickable rows)
- Quantity field ("x 1")
- **"Craft" button** (bottom left)
- "Goodbye" button (bottom right)

**Requirements:**
1. Find frame hashes for: Craft button, item list entries, quantity field
2. Select the item (click the row) → sends a selection message
3. Set quantity if > 1
4. Click the "Craft" button frame

**Known frame hashes** (from frame_aliases.json):
- `3613855137` = "Merchant" (main window)
- `1532320307` = "Merchant Buy Button"
- No craft-specific hashes found — we'd need to discover them at runtime

**Pros:**
- Replicates exact user interaction
- Already proven to work (Play button click succeeded)
- Doesn't depend on packet format or Transaction function addresses

**Cons:**
- Need to discover craft dialog frame hashes (runtime enumeration)
- Multiple steps: select item → set quantity → click craft
- Slower than direct packet (UI animation delays)
- Need to handle vendor dialog opening/state

### Approach B: GWCA Merchant::TransactItems()
Use GWCA's merchant transaction API directly.

**GWCA API:**
```cpp
// MerchantMgr.h
namespace GW::Merchant {
    enum class TransactionType : uint32_t {
        CrafterBuy = 3,       // Crafting at consumables vendor
        MerchantBuy = 1,      // Regular merchant purchase
        TraderBuy = 12,       // Material trader buy
        TraderSell = 13,      // Material trader sell
    };

    struct TransactionInfo {
        uint32_t  item_count = 0;
        uint32_t *item_ids = nullptr;
        uint32_t *item_quantities = nullptr;
    };

    GWCA_API bool TransactItems();
    GWCA_API bool RequestQuote(TransactionType type, uint32_t item_id);
    GWCA_API size_t GetMerchantItems(TransactionType type, ...);
};
```

**UIMessage structs for transactions:**
```cpp
// kSendMerchantRequestQuote
struct { TransactionType type; uint32_t item_id; };

// kSendMerchantTransactItem
struct {
    TransactionType type;
    uint32_t h0004;
    QuoteInfo give;       // { unknown, item_count, *item_ids }
    uint32_t gold_recv;
    QuoteInfo recv;       // { unknown, item_count, *item_ids }
};
```

**UIMessage IDs (from UIMessages.h):**
- `kVendorWindow` (0x100000B5) — vendor window opened
- `kVendorItems` (0x100000B9) — vendor item list populated
- `kVendorTransComplete` (0x100000BB) — transaction completed
- `kVendorQuote` (0x100000BD) — quote response received

**Pros:**
- Higher-level API, handles internal complexity
- Uses the game's own transaction system properly
- Well-tested in GWToolbox (MaterialsWindow.cpp uses it)

**Cons:**
- Requires GWCA GW::Initialize (breaks rendering hook)
- Or needs manual population of more GWCA data pointers
- `TransactItems()` is at an unknown RVA in gwca.dll
- May have the same issues as our current packet approach

### Approach C: Direct Game Transaction Function
Call the game's own `TransactionFunction` directly via shellcode, bypassing both GWCA and the BotsHub `CommandCraftItemEx2`.

The game's transaction function is already scanned by BotsHub (used by `CommandCraftItemEx2` assembly). We could build shellcode that calls it directly via the rendering hook, with properly structured arguments.

**Pros:**
- No GWCA dependency
- Direct game function call, same as what the UI does
- Can be called from rendering hook (game thread)

**Cons:**
- Need to reverse the exact Transaction function signature
- Same complexity as current approach but via shellcode instead of ASM commands
- Still depends on correct struct layout

---

## Recommended Investigation Path

### Phase 1: Runtime Frame Discovery (Approach A)
Since the Play button click mechanism is proven, enumerate the craft dialog frames at runtime:

1. Open the craft vendor dialog (Eyja) manually or via existing bot code
2. Dump ALL visible frames and their hashes using `GetFrameByHash` enumeration
3. Identify the "Craft" button, item list frames, and tab frames
4. Test clicking the Craft button frame after selecting an item

**Test script needed:** `test_craft_frame_discovery.au3`
- Walk to Eyja, open dialog
- Enumerate frame array, dump all visible frames with parent/child info
- Find frames matching the craft dialog layout
- Document discovered hashes

### Phase 2: Craft Button Click Test
Once we have the Craft button frame hash:
1. Ensure the correct item is selected in the vendor list
2. Use GWCA ButtonClick on the Craft button frame
3. Verify crafting occurs (gold decreases, item appears)

### Phase 3: Evaluate vs Fix Existing Approach
Compare the UI click approach with diagnosing and fixing the existing `CraftItemSafe`:
- If UI click works reliably → integrate into Utils-Maintenance.au3
- If existing packet approach just needs a minor fix → might be simpler

---

## Key Data Points for Investigation

### Frame Discovery Results (2026-03-28)

**Merchant root**: hash=3613855137, id=102

**Dialog frame hierarchy** (parent=104, the main content area):
| Hash | ID | childOff | Cbs | Likely Element |
|---|---|---|---|---|
| 1517397806 | 790 | 0 | 4 | **Craft tab** |
| 3738633661 | 791 | 1 | 4 | **Sell tab** |
| 835947118 | 553 | 2 | 4 | **Craft action button** (bottom-left) |
| 1599557336 | 505 | 3 | 1 | Label/header text |
| 458991075 | 27 | 4 | 1 | Item detail area |
| 1214056301 | 504 | 5 | 4 | **Item list scrollable area** |
| 3068881268 | 554 | 6 | 4 | **Goodbye button** (bottom-right) |

**Item rows** (repeating pattern, parent=960/964/966):
- Each item row has 6 child frames with consistent hashes:
  - hash=1852904459 (childOff=4) — item entry container
  - hash=3963670690 (childOff=5) — item icon/checkbox
  - hash=1820256588 (childOff=0) — item text/label
  - hash=3282622945 (childOff=3) — material info
  - hash=3216064980 (childOff=1) — price/cost info
- Items have sequential childOff values (112-150+)
- Some item children are disabled (greyed out = can't craft)

**Tab structure**: Two tabs (Craft=childOff 0, Sell=childOff 1) — the "Sell" tab (id=791) is visible but inactive, "Craft" tab (id=790) is the active tab.

**NOTE**: The Craft action button (hash=835947118) and Goodbye button (hash=3068881268) are the key targets for UI-based crafting.

### GWCA + BotsHub Coexistence Issue
GWCA injection (for ButtonClick) can cause crashes when combined with BotsHub hooks, especially during:
- BotsHub re-initialization on the same process
- Opening vendor dialogs while GWCA is loaded
- Map transitions

**Mitigation**: `ClickFrameButton()` has been updated to use a **native approach** — no gwca.dll needed. It computes the frame context via `_GetFrameContext()` (pure memory reads: `[frame+0x128] - 0x128`), then calls the game's own `SendFrameUIMsg` directly via shellcode. This works through the BotsHub command queue without any external DLL.

**GWCA is NOT needed at all.** The native `ClickFrameButton()` works for both char select (Play button) and in-game (vendor buttons). At char select, commands execute via the rendering hook. In-game, both MainProc and RenderingModProc process the queue.

### Craft Button Click Status (2026-03-28)
- Command queue confirmed working on fresh clients (no gwca.dll)
- ClickFrameButton updated to native approach (no gwca.dll in-game)
- Need to test: select item row → click Craft button → verify gold decreases
- NPC navigation to Eyja needs improvement (coords/timing issues)
- Craft button hash=835947118 identified but needs click verification

### GWCA DLL Function RVAs (from disassembly)
| Function | RVA | Notes |
|---|---|---|
| ButtonClick | +0x255E0 | Takes Frame*, proven working |
| ButtonFrame::Click | +0x16660 | __thiscall ECX=Frame* |
| SendFrameUIMessage | +0x274D0 | Needs +0x8A3A0 populated |
| GetFrameById | +0x25CC0 | Needs +0x8A3B0 (frame table) |
| TransactItems | Unknown | Need to find in gwca.dll |
| RequestQuote | Unknown | Need to find in gwca.dll |

### Game Function Addresses (relative to game base)
| Function | Offset | Label |
|---|---|---|
| SendFrameUIMsg | +0x2286D0 | SendFrameUIMsg / Action |
| Transaction | scanned | Transaction (BotsHub label) |
| RequestQuote | scanned | RequestQuote (BotsHub label) |

---

## Files Reference
- `GWA Censured/lib/custom/Utils-Maintenance.au3` — CraftItemSafe, buyConsumablesInEmbarkBeach
- `GWA Censured/lib/custom/GWA2_Crafting.au3` — CRAFT_ITEM_STRUCT, CommandCraftItemEx2 ASM
- `GWA Censured/lib/botshub/GWA2_Assembly.au3` — CommandCraftItem, CommandTraderBuy ASM
- `toolbox/GWToolboxpp-master/Dependencies/GWCA/include/GWCA/Managers/MerchantMgr.h` — GWCA merchant API
- `toolbox/GWToolboxpp-master/Dependencies/GWCA/include/GWCA/Constants/UIMessages.h` — Vendor UIMessages
- `toolbox/GWToolboxpp-master/GWToolboxdll/Windows/MaterialsWindow.cpp` — GWToolbox usage example
