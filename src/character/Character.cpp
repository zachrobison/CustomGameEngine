#include "Character.h"

Character::Character() {
    // All scales default to 1 — user adjusts from here
    for (int i = 0; i < (int)BodyPart::COUNT; i++)
        scales[i].v = { 1.f, 1.f, 1.f };

    // Default clothing colours per slot
    clothing[(int)ClothingSlot::Head ].color = { 0.25f, 0.25f, 0.25f };
    clothing[(int)ClothingSlot::Torso].color = { 0.20f, 0.40f, 0.80f };
    clothing[(int)ClothingSlot::Arms ].color = { 0.20f, 0.40f, 0.80f };
    clothing[(int)ClothingSlot::Legs ].color = { 0.18f, 0.18f, 0.35f };
    clothing[(int)ClothingSlot::Feet ].color = { 0.25f, 0.15f, 0.10f };
}
