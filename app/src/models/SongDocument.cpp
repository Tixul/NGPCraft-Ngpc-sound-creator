#include "models/SongDocument.h"
#include "models/TrackerDocument.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

// ============================================================
// Constructor
// ============================================================

SongDocument::SongDocument(QObject* parent)
    : QObject(parent)
{
    // Start with one default pattern and order = [0]
    auto* pat = new TrackerDocument(this);
    patterns_.push_back(pat);
    order_.push_back(0);
}

// ============================================================
// Pattern bank
// ============================================================

int SongDocument::pattern_count() const {
    return static_cast<int>(patterns_.size());
}

TrackerDocument* SongDocument::pattern(int index) {
    if (index < 0 || index >= pattern_count()) return nullptr;
    return patterns_[static_cast<size_t>(index)];
}

const TrackerDocument* SongDocument::pattern(int index) const {
    if (index < 0 || index >= pattern_count()) return nullptr;
    return patterns_[static_cast<size_t>(index)];
}

int SongDocument::add_pattern() {
    if (pattern_count() >= kMaxPatterns) return -1;
    auto* pat = new TrackerDocument(this);
    patterns_.push_back(pat);
    emit pattern_list_changed();
    return pattern_count() - 1;
}

int SongDocument::clone_pattern(int source_index) {
    if (source_index < 0 || source_index >= pattern_count()) return -1;
    if (pattern_count() >= kMaxPatterns) return -1;

    auto* src = patterns_[static_cast<size_t>(source_index)];
    auto* dst = new TrackerDocument(this);

    // Copy via JSON round-trip (simple and reliable)
    QByteArray data = src->to_json();
    dst->from_json(data);

    patterns_.push_back(dst);
    emit pattern_list_changed();
    return pattern_count() - 1;
}

void SongDocument::remove_pattern(int index) {
    // Refuse to remove the last pattern
    if (pattern_count() <= 1) return;
    if (index < 0 || index >= pattern_count()) return;

    // Delete the pattern object
    delete patterns_[static_cast<size_t>(index)];
    patterns_.erase(patterns_.begin() + index);

    // Fix active index
    if (active_index_ >= pattern_count())
        active_index_ = pattern_count() - 1;

    // Fix order list: remove references and adjust indices
    sanitize_order();

    emit pattern_list_changed();
    emit order_changed();
    emit active_pattern_changed(active_index_);
}

// ============================================================
// Active pattern
// ============================================================

void SongDocument::set_active_pattern(int index) {
    if (index < 0 || index >= pattern_count()) return;
    if (index == active_index_) return;
    active_index_ = index;
    emit active_pattern_changed(active_index_);
}

TrackerDocument* SongDocument::active_pattern() {
    return patterns_[static_cast<size_t>(active_index_)];
}

// ============================================================
// Order list
// ============================================================

void SongDocument::order_insert(int position, int pattern_index) {
    if (static_cast<int>(order_.size()) >= kMaxOrderLength) return;
    if (pattern_index < 0 || pattern_index >= pattern_count()) return;
    position = std::clamp(position, 0, static_cast<int>(order_.size()));
    order_.insert(order_.begin() + position, pattern_index);
    // Fix loop point if insertion is before or at loop
    if (position <= loop_point_ && static_cast<int>(order_.size()) > 1)
        loop_point_++;
    emit order_changed();
}

void SongDocument::order_remove(int position) {
    // Refuse to remove the last entry
    if (order_.size() <= 1) return;
    if (position < 0 || position >= static_cast<int>(order_.size())) return;
    order_.erase(order_.begin() + position);
    // Fix loop point
    if (loop_point_ >= static_cast<int>(order_.size()))
        loop_point_ = static_cast<int>(order_.size()) - 1;
    else if (position < loop_point_)
        loop_point_--;
    emit order_changed();
}

void SongDocument::order_move_up(int position) {
    if (position <= 0 || position >= static_cast<int>(order_.size())) return;
    std::swap(order_[static_cast<size_t>(position)],
              order_[static_cast<size_t>(position - 1)]);
    // Adjust loop point if it was one of the swapped positions
    if (loop_point_ == position)
        loop_point_ = position - 1;
    else if (loop_point_ == position - 1)
        loop_point_ = position;
    emit order_changed();
}

void SongDocument::order_move_down(int position) {
    if (position < 0 || position >= static_cast<int>(order_.size()) - 1) return;
    std::swap(order_[static_cast<size_t>(position)],
              order_[static_cast<size_t>(position + 1)]);
    // Adjust loop point if it was one of the swapped positions
    if (loop_point_ == position)
        loop_point_ = position + 1;
    else if (loop_point_ == position + 1)
        loop_point_ = position;
    emit order_changed();
}

void SongDocument::order_set_entry(int position, int pattern_index) {
    if (position < 0 || position >= static_cast<int>(order_.size())) return;
    if (pattern_index < 0 || pattern_index >= pattern_count()) return;
    order_[static_cast<size_t>(position)] = pattern_index;
    emit order_changed();
}

// ============================================================
// Loop point
// ============================================================

void SongDocument::set_loop_point(int order_position) {
    order_position = std::clamp(order_position, 0,
                                std::max(0, static_cast<int>(order_.size()) - 1));
    if (order_position == loop_point_) return;
    loop_point_ = order_position;
    emit order_changed();
}

// ============================================================
// Sanitize order (after pattern removal)
// ============================================================

void SongDocument::sanitize_order() {
    int pc = pattern_count();
    // Remove entries referencing deleted patterns, adjust indices
    for (auto it = order_.begin(); it != order_.end(); ) {
        if (*it >= pc) {
            it = order_.erase(it);
        } else {
            ++it;
        }
    }
    // Ensure at least one entry
    if (order_.empty())
        order_.push_back(0);
    // Fix loop point
    if (loop_point_ >= static_cast<int>(order_.size()))
        loop_point_ = static_cast<int>(order_.size()) - 1;
}

// ============================================================
// Serialization — .ngps (song) format
// ============================================================

QByteArray SongDocument::to_json() const {
    QJsonObject root;
    root["version"] = 1;

    // Patterns array — reuse TrackerDocument's JSON
    QJsonArray pat_array;
    for (const auto* pat : patterns_) {
        QJsonDocument pdoc = QJsonDocument::fromJson(pat->to_json());
        pat_array.append(pdoc.object());
    }
    root["patterns"] = pat_array;

    // Order list
    QJsonArray ord_array;
    for (int idx : order_)
        ord_array.append(idx);
    root["order"] = ord_array;

    // Loop point
    root["loop_point"] = loop_point_;

    QJsonDocument doc(root);
    return doc.toJson(QJsonDocument::Compact);
}

bool SongDocument::from_json(const QByteArray& data) {
    QJsonParseError err;
    QJsonDocument jdoc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) return false;

    QJsonObject root = jdoc.object();
    if (!root.contains("patterns") || !root.contains("order")) return false;

    QJsonArray pat_array = root["patterns"].toArray();
    if (pat_array.isEmpty()) return false;
    if (pat_array.size() > kMaxPatterns) return false;

    QJsonArray ord_array = root["order"].toArray();
    if (ord_array.isEmpty()) return false;
    if (ord_array.size() > kMaxOrderLength) return false;

    // Load patterns
    // Delete existing
    for (auto* p : patterns_) delete p;
    patterns_.clear();

    for (int i = 0; i < pat_array.size(); ++i) {
        auto* pat = new TrackerDocument(this);
        QJsonDocument pdoc(pat_array[i].toObject());
        QByteArray pdata = pdoc.toJson(QJsonDocument::Compact);
        if (!pat->from_json(pdata)) {
            // Cleanup on failure
            delete pat;
            for (auto* p : patterns_) delete p;
            patterns_.clear();
            // Restore defaults
            patterns_.push_back(new TrackerDocument(this));
            order_ = {0};
            active_index_ = 0;
            loop_point_ = 0;
            emit pattern_list_changed();
            emit order_changed();
            emit active_pattern_changed(0);
            return false;
        }
        patterns_.push_back(pat);
    }

    // Load order
    order_.clear();
    int pc = pattern_count();
    for (int i = 0; i < ord_array.size(); ++i) {
        int idx = ord_array[i].toInt(0);
        if (idx >= 0 && idx < pc)
            order_.push_back(idx);
    }
    if (order_.empty())
        order_.push_back(0);

    // Load loop point
    loop_point_ = root["loop_point"].toInt(0);
    loop_point_ = std::clamp(loop_point_, 0,
                             std::max(0, static_cast<int>(order_.size()) - 1));

    // Reset active index
    active_index_ = 0;

    emit pattern_list_changed();
    emit order_changed();
    emit active_pattern_changed(0);
    return true;
}

bool SongDocument::import_ngpat(const QByteArray& data) {
    // Import a single .ngpat file as a song with 1 pattern
    auto* pat = new TrackerDocument(this);
    if (!pat->from_json(data)) {
        delete pat;
        return false;
    }

    // Replace everything
    for (auto* p : patterns_) delete p;
    patterns_.clear();

    patterns_.push_back(pat);
    order_ = {0};
    active_index_ = 0;
    loop_point_ = 0;

    emit pattern_list_changed();
    emit order_changed();
    emit active_pattern_changed(0);
    return true;
}
