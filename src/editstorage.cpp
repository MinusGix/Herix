#include "editstorage.hpp"
#include <map>
#include <cassert>

EditStorageItem::EditStorageItem (FilePosition t_pos, Buffer t_data) {
    pos = t_pos;
    data = t_data;
}



EditStorage::EditStorage () {}

size_t EditStorage::getCurrentLimit () const {
    return current_limit;
}

/// Returns current_end's literal value
std::optional<size_t> EditStorage::getCurrentEndValue () const {
    return current_end;
}

/// Checks if current_end is nullopt and if it is, gets the size of the array, otherwise returns the value
size_t EditStorage::getCurrentEnd () const {
    if (current_end.has_value()) {
        return current_end.value();
    } else {
        return edits.size();
    }
}

/// Get the amount of stored data, includes past and future. This is NOT the byte count, but rather how many pairs of 
/// FilePosition,Buffer 's there are.
size_t EditStorage::getEntryCount() const {
    return edits.size();
}
/// Get the amount of stored data in the past. This is NOT the byte count, but rather how many pairs of 
/// FilePosition,Buffer 's there are
size_t EditStorage::getPastEntryCount() const {
    return getCurrentEnd();
}
/// Get the amount of stored data in the 'future'. This is NOT the byte count, but rather how many pairs of
/// FilePosition,Buffer 's there are.
size_t EditStorage::getFutureEntryCount() const {
    return edits.size() - getCurrentEnd();
}

/// Get the amount of bytes that are currently stored in editstorage. Checks all of history.
/// Is NOT the amount of bytes written over all of time, since data may have been shrunk down and this will change value.
size_t EditStorage::getBytesStored() const {
    size_t count = 0;
    for (const EditStorageItem& item : edits) {
        count += item.data.size();
    }
    return count;
}
size_t EditStorage::getBytesStoredPast() const {
    size_t count = 0;
    size_t end = getCurrentEnd();

    for (size_t i = 0; i < end; i++) {
        count += edits.at(i).data.size();
    }

    return count;
}
size_t EditStorage::getBytesStoredFuture() const {
    size_t count = 0;
    for (size_t i = getCurrentEnd(); i < edits.size(); i++) {
        count += edits.at(i).data.size();
    }
    return count;
}

/// Returns the number of bytes written in all of EditStorage's 'history'. Includes things that were undone.
unsigned long EditStorage::getBytesWritten() const {
    return bytes_written;
}
unsigned long EditStorage::getBytesWrittenAllTime() const {
    return bytes_written_alltime;
}

/// Returns the number of bytes that have been edited at the current time. If two edits were done at the same position,
/// it considers them the same. I couldn't think of a better name.
size_t EditStorage::getBytesFilledIn() const {
    // TODO: make this better.
    // A simple modification would be to remove count and just use found.size()
    // But a better modification would be nice.
    size_t count = 0;
    std::vector<size_t> found;
    size_t end = getCurrentEnd();

    for (size_t i = 0; i < end; i++) {
        EditStorageItem item = edits.at(end - i - 1);

        bool should_continue = false;
        for (const size_t index : found) {
            if (index == item.pos) {
                should_continue = true;
                break;
            }
        }

        if (should_continue) {
            continue;
        }

        for (size_t j = 0; j < item.data.size(); j++) {
            found.push_back(item.pos + j);
        }
        count += item.data.size();
    }

    return count;
}

// == Editing ==

/// Set the byte at this position. If you're setting multiple bytes at once, use #edit(FilePosition pos, Buffer data)
/// If there is future edits (aka you've undone, and moved back), then this will erase those and replace it with this edit
void EditStorage::edit (FilePosition pos, Byte value) {
    editMultiple(pos, Buffer{value});
}

/// Sets multiple bytes, starting at the position. (data[0] is at pos, data[1] is at pos+1, data[n] is at pos+n)
/// If there is future edits (aka you've undone, and moved back), then this will erase those and replace it with this edit
void EditStorage::editMultiple (FilePosition pos, Buffer data) {
    assert(current_limit == 0 || current_limit <= current_end);

    if (!current_end.has_value()) {
        // Just push to end if the current_end.. is at the end :)
        edits.push_back(EditStorageItem(pos, data));
    } else {
        assert(current_end.value() <= static_cast<size_t>(std::numeric_limits<std::vector<EditStorageItem>::difference_type>::max()));

        // I dislike this. Current_end is unsigned long (well size_t), but difference_type is signed long (well, I imagine it's ssize_t)
        // Indexing uses size_type (aka size_t), but insert uses the signed version...
        edits.insert(edits.begin() + static_cast<std::vector<EditStorageItem>::difference_type>(current_end.value()), EditStorageItem(pos, data));
    }
    bytes_written += data.size();
    bytes_written_alltime += data.size();
}

// == Reading ==

// Reminder that these are for checking what's written in the edit data, the main class will provide the actual values with a function
// if they haven't been edited

/// This looks for something which directly sets at the address, and which has a buffer of size 1
/// So this basically looks for when something assigns to that position itself (like a result of using edit with one value)
std::optional<Byte> EditStorage::readSingleAssignment (FilePosition pos) const {
    size_t end = getCurrentEnd();

    for (size_t i = 0; i < end; i++) {
        EditStorageItem item = edits.at(end - i - 1);
        if (pos == item.pos && item.data.size() == 1) {
            return item.data[0];
        }
    }

    return std::nullopt;
}

std::optional<Byte> EditStorage::read (FilePosition pos) const {
    size_t end = getCurrentEnd();

    for (size_t i = 0; i < end; i++) {
        EditStorageItem item = edits.at(end - i - 1);
        // The buffer is of a variable size so it might be setting at the position we want, but not exactly on it
        if (pos >= item.pos && pos < (item.pos + item.data.size())) {
            size_t rpos = pos - item.pos;
            return item.data.at(rpos);
        }
    }
    return std::nullopt;
}

/// Reads multiple bytes
std::vector<std::optional<Byte>> EditStorage::readMultiple (FilePosition pos, size_t size) const {
    // TODO: replace this with better code
    std::vector<std::optional<Byte>> ret;

    for (size_t i = 0; i < size; i++) {
        ret.push_back(read(pos+i));
    }

    assert(ret.size() == size);

    return ret;
}


// == Undoing/Redoing ==

/// Undoes the 'latest' edit at our point in history. If it was a single byte written then it undoes that, if it was a group, then it undoes the entire group.
/// Returns the pair of FilePosition and Buffer that was stored
/// Do note that there is an inbuilt redo, so you don't need to store it for your own redo.
std::optional<EditStorageItem> EditStorage::undoR () {
    size_t end = EditStorage::getCurrentEnd();

    if (canUndo()) {
        current_end = std::make_optional(end - 1);
        EditStorageItem const& item = edits.at(end-1);
        bytes_written -= item.data.size();
        return edits.at(end-1);
    } else {
        return std::nullopt;
    }
}

std::optional<size_t> EditStorage::undoP () {
    std::optional<EditStorageItem> item = undoR();

    if (item.has_value()) {
        return std::make_optional(item.value().pos);
    } else {
        return std::nullopt;
    }
}

/// Undoes the latest edit call. If it was a single byte written then it undoes that, if it was a group, then it undoes the entire group.
void EditStorage::undo () {
    // Just call it and discard the return.
    undoR();
}

bool EditStorage::canUndo () const {
    size_t end = getCurrentEnd();
    return end != 0 && end > current_limit;
}

// Redo and return Position and values stored their
std::optional<EditStorageItem> EditStorage::redoR () {
    size_t end = getCurrentEnd();

    if (canRedo()) {
        if ((end+1) == edits.size()) {
            // Reset it back to nullopt if we go to the end.
            // If we didn't do this, then why not just track the value constantly?
            // I mean that's a possiblity, but we don't.
            current_end = std::nullopt;
        } else {
            current_end = std::make_optional(end + 1);
        }

        // Since end is at the *end* of the current data, this will return the one we redo
        EditStorageItem item = edits.at(end);
        bytes_written += item.data.size();
        return std::make_optional(item);
    } else {
        return std::nullopt;
    }
}

// Redo and return position
std::optional<size_t> EditStorage::redoP () {
    std::optional<EditStorageItem> item = redoR();

    if (item.has_value()) {
        return std::make_optional(item.value().pos);
    } else {
        return std::nullopt;
    }
}

void EditStorage::redo () {
    // Just call it and discard the return
    redoR();
}

bool EditStorage::canRedo () const {
    size_t end = getCurrentEnd();
    return end < edits.size();
}

/// Joins edits at the same position to only have the most recent version
/// This WILL STOP undo from going into the collated section, since it will be funky there
/// Does not affect anything in the 'future'
void EditStorage::collateEdits () {
    throw std::logic_error("This function is not yet implemented (collateEdits)");
    if (!current_end.has_value()) {
        return;
    }
    // TODO: finish this

    size_t end = getCurrentEnd();

    for (size_t i = 0; i < end; i++) {

    }
}

void EditStorage::clear () noexcept {
    bytes_written_alltime = 0;
    bytes_written = 0;

    clearNotStats();
}

/// Same as clear but doesn't do stats
void EditStorage::clearNotStats () noexcept {
    current_end = std::nullopt;
    current_limit = 0;

    edits.clear();
}


// === Testing ===

#ifdef DEBUG

#include <iostream>
void test_editstorage () {
    EditStorage e;
    // = Test without elements

    // Test current end
    assert(e.getCurrentEndValue() == std::nullopt);
    assert(e.getCurrentEnd() == 0);

    // Test entry counting
    assert(e.getEntryCount() == 0);
    assert(e.getPastEntryCount() == 0);
    assert(e.getFutureEntryCount() == 0);

    // Test byte stored counting
    assert(e.getBytesStored() == 0);
    assert(e.getBytesStoredPast() == 0);
    assert(e.getBytesStoredFuture() == 0);

    // Test byte written counting
    assert(e.getBytesWritten() == 0);
    assert(e.getBytesWrittenAllTime() == 0);

    // Test read instructions
    assert(e.readSingleAssignment(0) == std::nullopt);
    assert(e.read(0) == std::nullopt);

    // Test filled in bytes
    assert(e.getBytesFilledIn() == 0);

    std::vector<std::optional<Byte>> readm1 = e.readMultiple(0, 2);
    assert(readm1.size() == 2);
    assert(!readm1.at(0).has_value());
    assert(!readm1.at(1).has_value());

    assert(e.readMultiple(0, 0).size() == 0);

    // Test undo/redo
    assert(e.undoR() == std::nullopt);
    assert(e.redoR() == std::nullopt);
    assert(e.undoP() == std::nullopt);
    assert(e.redoP() == std::nullopt);

    // = Test with a single element
    e.edit(0, 4);
    // Test read instructions
    assert(e.read(0).has_value());
    assert(e.read(0).value() == 4);

    std::vector<std::optional<Byte>> readm2 = e.readMultiple(0, 2);
    assert(readm2.size() == 2);
    assert(readm2.at(0).has_value());
    assert(readm2.at(0).value() == 4);
    assert(!readm2.at(1).has_value());

    // Test current end
    assert(e.getCurrentEndValue() == std::nullopt);
    assert(e.getCurrentEnd() == 1);

    // Test entry count
    assert(e.getEntryCount() == 1);
    assert(e.getPastEntryCount() == 1);
    assert(e.getFutureEntryCount() == 0);

    // Test byte stored
    assert(e.getBytesStored() == 1);
    assert(e.getBytesStoredPast() == 1);
    assert(e.getBytesStoredFuture() == 0);

    // Test bytes written
    assert(e.getBytesWritten() == 1);
    assert(e.getBytesWrittenAllTime() == 1);

    // Test filled in bytes
    assert(e.getBytesFilledIn() == 1);

    // = Test after writing a byte over the element
    e.edit(0, 9);
    // Test read instructions
    assert(e.read(0).has_value());
    assert(e.read(0).value() == 9);

    std::vector<std::optional<Byte>> readm2a = e.readMultiple(0, 2);
    assert(readm2a.size() == 2);
    assert(readm2a.at(0).has_value());
    assert(readm2a.at(0).value() == 9);
    assert(!readm2a.at(1).has_value());

    // Test current end
    assert(e.getCurrentEndValue() == std::nullopt);
    assert(e.getCurrentEnd() == 2);

    // Test entry count
    assert(e.getEntryCount() == 2);
    assert(e.getPastEntryCount() == 2);
    assert(e.getFutureEntryCount() == 0);

    // Test byte stored
    assert(e.getBytesStored() == 2);
    assert(e.getBytesStoredPast() == 2);
    assert(e.getBytesStoredFuture() == 0);

    // Test bytes written
    assert(e.getBytesWritten() == 2);
    assert(e.getBytesWrittenAllTime() == 2);

    // Test filled in bytes
    assert(e.getBytesFilledIn() == 1);

    // = Test with multiple elements
    e.edit(2, 6);

    // test reading
    std::vector<std::optional<Byte>> readm3 = e.readMultiple(0, 3);
    assert(readm3.size() == 3);
    assert(readm3.at(0).has_value());
    assert(readm3.at(0).value() == 9);
    assert(!readm3.at(1).has_value());
    assert(readm3.at(2).has_value());
    assert(readm3.at(2).value() == 6);

    // Test current end
    assert(e.getCurrentEndValue() == std::nullopt);
    assert(e.getCurrentEnd() == 3);

    // Test entry count
    assert(e.getEntryCount() == 3);
    assert(e.getPastEntryCount() == 3);
    assert(e.getFutureEntryCount() == 0);

    // Test bytes stored
    assert(e.getBytesStored() == 3);
    assert(e.getBytesStoredPast() == 3);
    assert(e.getBytesStoredFuture() == 0);

    // Test bytes written
    assert(e.getBytesWritten() == 3);
    assert(e.getBytesWrittenAllTime() == 3);

    // Test filled in bytes
    assert(e.getBytesFilledIn() == 2);


    // = Test undo
    std::optional<EditStorageItem> u1 = e.undoR();
    assert(u1.has_value());
    assert(u1.value().pos == 2);
    assert(u1.value().data.size() == 1);
    assert(u1.value().data.at(0) == 6);

    // test reading
    std::vector<std::optional<Byte>> readm4 = e.readMultiple(0, 3);
    assert(readm4.size() == 3);
    assert(readm4.at(0).has_value());
    assert(readm4.at(0).value() == 9);
    assert(!readm4.at(1).has_value());
    assert(!readm4.at(2).has_value());

    // Test current end
    assert(e.getCurrentEndValue() == 2);
    assert(e.getCurrentEnd() == 2);

    // Test entry count
    assert(e.getEntryCount() == 3);
    assert(e.getPastEntryCount() == 2);
    assert(e.getFutureEntryCount() == 1);

    // Test bytes stored
    assert(e.getBytesStored() == 3);
    assert(e.getBytesStoredPast() == 2);
    assert(e.getBytesStoredFuture() == 1);

    // Test bytes written
    assert(e.getBytesWritten() == 2);
    assert(e.getBytesWrittenAllTime() == 3);

    // Test filled in bytes
    assert(e.getBytesFilledIn() == 1);

    // Undo again

    std::optional<EditStorageItem> u2 = e.undoR();
    assert(u2.has_value());
    assert(u2.value().pos == 0);
    assert(u2.value().data.size() == 1);
    assert(u2.value().data.at(0) == 9);

    // test reading
    std::vector<std::optional<Byte>> readm5 = e.readMultiple(0, 3);
    assert(readm5.size() == 3);
    assert(readm5.at(0).has_value());
    assert(readm5.at(0).value() == 4);
    assert(!readm5.at(1).has_value());
    assert(!readm5.at(2).has_value());

    // Test current end
    assert(e.getCurrentEndValue() == 1);
    assert(e.getCurrentEnd() == 1);

    // Test entry count
    assert(e.getEntryCount() == 3);
    assert(e.getPastEntryCount() == 1);
    assert(e.getFutureEntryCount() == 2);

    // Test bytes stored
    assert(e.getBytesStored() == 3);
    assert(e.getBytesStoredPast() == 1);
    assert(e.getBytesStoredFuture() == 2);

    // Test bytes written
    assert(e.getBytesWritten() == 1);
    assert(e.getBytesWrittenAllTime() == 3);

    // Test filled in bytes
    assert(e.getBytesFilledIn() == 1);

    // Undo again

    std::optional<EditStorageItem> u3 = e.undoR();
    assert(u3.has_value());
    assert(u3.value().pos == 0);
    assert(u3.value().data.size() == 1);
    assert(u3.value().data.at(0) == 4);

    // test reading
    std::vector<std::optional<Byte>> readm6 = e.readMultiple(0, 3);
    assert(readm6.size() == 3);
    assert(!readm6.at(0).has_value());
    assert(!readm5.at(1).has_value());
    assert(!readm5.at(2).has_value());

    // Test current end
    assert(e.getCurrentEndValue() == 0);
    assert(e.getCurrentEnd() == 0);

    // Test entry count
    assert(e.getEntryCount() == 3);
    assert(e.getPastEntryCount() == 0);
    assert(e.getFutureEntryCount() == 3);

    // Test bytes stored
    assert(e.getBytesStored() == 3);
    assert(e.getBytesStoredPast() == 0);
    assert(e.getBytesStoredFuture() == 3);

    // Test bytes written
    assert(e.getBytesWritten() == 0);
    assert(e.getBytesWrittenAllTime() == 3);

    // Test filled in bytes
    assert(e.getBytesFilledIn() == 0);

    // Check that undo doesn't do anything
    assert(!e.undoR().has_value());

    // = Redo
    std::optional<EditStorageItem> r1 = e.redoR();
    assert(r1.has_value());
    assert(r1.value().pos == 0);
    assert(r1.value().data.size() == 1);
    assert(r1.value().data.at(0) == 4);

    // test reading
    std::vector<std::optional<Byte>> readm7 = e.readMultiple(0, 3);
    assert(readm7.size() == 3);
    assert(readm7.at(0).has_value());
    assert(readm7.at(0).value() == 4);
    assert(!readm7.at(1).has_value());
    assert(!readm7.at(2).has_value());

    // Test current end
    assert(e.getCurrentEndValue() == 1);
    assert(e.getCurrentEnd() == 1);

    // Test entry count
    assert(e.getEntryCount() == 3);
    assert(e.getPastEntryCount() == 1);
    assert(e.getFutureEntryCount() == 2);

    // Test bytes stored
    assert(e.getBytesStored() == 3);
    assert(e.getBytesStoredPast() == 1);
    assert(e.getBytesStoredFuture() == 2);

    // Test bytes written
    assert(e.getBytesWritten() == 1);
    assert(e.getBytesWrittenAllTime() == 3);

    // Test filled in bytes
    assert(e.getBytesFilledIn() == 1);

    // = Redo again
    std::optional<EditStorageItem> r2 = e.redoR();
    assert(r2.has_value());
    assert(r2.value().pos == 0);
    assert(r2.value().data.size() == 1);
    assert(r2.value().data.at(0) == 9);

    // test reading
    std::vector<std::optional<Byte>> readm8 = e.readMultiple(0, 3);
    assert(readm8.size() == 3);
    assert(readm8.at(0).has_value());
    assert(readm8.at(0).value() == 9);
    assert(!readm8.at(1).has_value());
    assert(!readm8.at(2).has_value());

    // Test current end
    assert(e.getCurrentEndValue() == 2);
    assert(e.getCurrentEnd() == 2);

    // Test entry count
    assert(e.getEntryCount() == 3);
    assert(e.getPastEntryCount() == 2);
    assert(e.getFutureEntryCount() == 1);

    // Test bytes stored
    assert(e.getBytesStored() == 3);
    assert(e.getBytesStoredPast() == 2);
    assert(e.getBytesStoredFuture() == 1);

    // Test bytes written
    assert(e.getBytesWritten() == 2);
    assert(e.getBytesWrittenAllTime() == 3);

    // Test filled in bytes
    assert(e.getBytesFilledIn() == 1);

    // = Redo again
    std::optional<EditStorageItem> r3 = e.redoR();
    assert(r3.has_value());
    assert(r3.value().pos == 2);
    assert(r3.value().data.size() == 1);
    assert(r3.value().data.at(0) == 6);

    // test reading
    std::vector<std::optional<Byte>> readm9 = e.readMultiple(0, 3);
    assert(readm9.size() == 3);
    assert(readm9.at(0).has_value());
    assert(readm9.at(0).value() == 9);
    assert(!readm9.at(1).has_value());
    assert(readm9.at(2).has_value());
    assert(readm9.at(2).value() == 6);

    // Test current end
    assert(e.getCurrentEndValue() == std::nullopt);
    assert(e.getCurrentEnd() == 3);

    // Test entry count
    assert(e.getEntryCount() == 3);
    assert(e.getPastEntryCount() == 3);
    assert(e.getFutureEntryCount() == 0);

    // Test bytes stored
    assert(e.getBytesStored() == 3);
    assert(e.getBytesStoredPast() == 3);
    assert(e.getBytesStoredFuture() == 0);

    // Test bytes written
    assert(e.getBytesWritten() == 3);
    assert(e.getBytesWrittenAllTime() == 3);

    // Test filled in bytes
    assert(e.getBytesFilledIn() == 2);
}


#endif
