bool TryGetConsumableCraftRecipe(uint32_t targetModelId, ConsumableCraftRecipe& recipe) {
    ZeroMemory(&recipe, sizeof(recipe));
    switch (targetModelId) {
    case ItemModelIds::GRAIL_OF_MIGHT:
        recipe.fee = 250u;
        recipe.materialCount = 2u;
        recipe.materials[0] = {kMaterialIronIngot, 50u};
        recipe.materials[1] = {kMaterialDust, 50u};
        return true;
    case ItemModelIds::ESSENCE_OF_CELERITY:
        recipe.fee = 250u;
        recipe.materialCount = 2u;
        recipe.materials[0] = {kMaterialFeather, 50u};
        recipe.materials[1] = {kMaterialDust, 50u};
        return true;
    case ItemModelIds::ARMOR_OF_SALVATION:
        recipe.fee = 250u;
        recipe.materialCount = 2u;
        recipe.materials[0] = {kMaterialIronIngot, 50u};
        recipe.materials[1] = {kMaterialBone, 50u};
        return true;
    default:
        return false;
    }
}
