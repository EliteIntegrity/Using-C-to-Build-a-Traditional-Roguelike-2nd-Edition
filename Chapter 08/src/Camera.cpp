#include "Camera.h"

Camera::Camera(int cellSize)
    : cellSize_(cellSize)
{
}

int Camera::cellSize() const
{
    return cellSize_;
}

// The cell's middle goes to the middle of the view. Then the view is held
// inside the map: its left edge can't go below 0, or past the point where
// its right edge would leave the map, and the same for its top
void Camera::follow(Point cell)
{
    float mapW = static_cast<float>(MAP_W * cellSize_);
    float mapH = static_cast<float>(MAP_H * cellSize_);
    float centerX = (cell.x + 0.5f) * cellSize_;
    float centerY = (cell.y + 0.5f) * cellSize_;
    float maxLeft = SDL_max(0.0f, mapW - MAP_VIEW_W);
    float maxTop = SDL_max(0.0f, mapH - MAP_VIEW_H);
    left_ = SDL_clamp(centerX - MAP_VIEW_W / 2.0f, 0.0f, maxLeft);
    top_ = SDL_clamp(centerY - MAP_VIEW_H / 2.0f, 0.0f, maxTop);
}

SDL_FRect Camera::cellRect(Point cell) const
{
    float size = static_cast<float>(cellSize_);
    return { cell.x * size - left_, cell.y * size - top_, size, size };
}

bool Camera::shows(Point cell) const
{
    SDL_FRect rect = cellRect(cell);
    return rect.x + rect.w > 0.0f && rect.x < MAP_VIEW_W &&
           rect.y + rect.h > 0.0f && rect.y < MAP_VIEW_H;
}
