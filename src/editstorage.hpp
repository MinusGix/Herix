#ifndef FILE_SEEN_EDITSTORAGE
#define FILE_SEEN_EDITSTORAGE

#include "types.hpp"
#include <vector>
#include <optional>
namespace HerixLib {

class EditStorageItem {
    public:
    FilePosition pos;
    Buffer data;

    EditStorageItem (FilePosition t_pos, Buffer t_data);
};

class EditStorage {
    public:
    std::vector<EditStorageItem> edits;
    // TODO: think about whether you should just store this as a size_t and update it.
    /// The current_end of data, used to keep track of history with undo/redo
    /// If it's nullopt then it's at the end of the vector
    std::optional<size_t> current_end = std::nullopt;
    // TODO: add a way for undo functions to go into this.
    /// The current limit (leftmost) of undo. This is used due to operations which shrink memory usage
    /// (such as collateEdits) and undoing into those would produce strange undos, so I felt it better to disable them.
    size_t current_limit = 0;


    // Updated by edit operations
    unsigned long bytes_written_alltime = 0;
    // Updated by edit operations, undone when you undo an action
    // So this only includes in the past
    unsigned long bytes_written = 0;

    EditStorage ();

    size_t getCurrentLimit () const;

    // Current history position
    std::optional<size_t> getCurrentEndValue () const;
    size_t getCurrentEnd () const;

    // Entry count. The amount of elements in the edits.
    size_t getEntryCount () const;
    size_t getPastEntryCount() const;
    size_t getFutureEntryCount() const;

    // The amount of bytes that are stored.
    size_t getBytesStored () const;
    size_t getBytesStoredPast () const;
    size_t getBytesStoredFuture () const;

    // The amount of bytes written.
    unsigned long getBytesWritten () const;
    unsigned long getBytesWrittenAllTime () const;

    // The bytes filled in uniquely. Writes to the same position yield only 1
    size_t getBytesFilledIn () const;

    // TODO: add function to return # possible undos
    // TODO: add function to return # possible redos

    // Editing
    void edit (FilePosition pos, Byte value);
    void editMultiple (FilePosition pos, Buffer data);

    // Reading
    std::optional<Byte> readSingleAssignment (FilePosition pos) const;
    std::optional<Byte> read (FilePosition pos) const;
    std::vector<std::optional<Byte>> readMultiple (FilePosition pos, size_t size) const;

    // Undo / Redo
    std::optional<EditStorageItem> undoR ();
    std::optional<size_t> undoP ();
    void undo ();
    bool canUndo () const;
    std::optional<EditStorageItem> redoR ();
    std::optional<size_t> redoP ();
    void redo ();
    bool canRedo () const;


// == Other ==
    // TODO: add a function to join edits together to shrink space usage of the stored edits.
    // Would break undo, but that's fine if it's only ran when explcitly

    void clear () noexcept;
    void clearNotStats () noexcept;
};

void test_editstorage();

}

#endif
