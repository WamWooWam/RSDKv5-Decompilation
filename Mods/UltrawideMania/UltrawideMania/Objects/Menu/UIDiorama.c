#include "../../ModConfig.h"

ObjectUIDiorama *UIDiorama;

void UIDiorama_Create(void* data)
{
    RSDK_THIS(UIDiorama);

    Mod.Super(UIDiorama->classID, SUPER_CREATE, data);

    self->position.x -= extraWidth << 16;
}
