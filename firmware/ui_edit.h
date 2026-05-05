/*
 * ui_edit.h - Edit overlays for links and categories.
 * Used by the list screen on long-press, and by the "+" tab.
 */
#pragma once
#include "storage.h"
#include <functional>

namespace UIEdit {
  // Open the edit overlay for an existing link.
  // After save/delete, calls onClose() so the list can re-render.
  void editLink(Link* link, std::function<void()> onClose);

  // Edit existing category (or create new if isNew=true)
  void editCategory(Category* cat, bool isNew, std::function<void()> onClose);
}
