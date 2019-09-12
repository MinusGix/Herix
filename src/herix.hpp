#ifndef FILE_SEEN_HERIX
#define FILE_SEEN_HERIX

#include <optional>
#include <ctime>
#include <map>
#include <chrono>
#include <fstream>
#include <filesystem>

#include "types.hpp"
#include "editstorage.hpp"

// TODO: rename all uses of index to 'id' or some related. Do this in the herix.cpp file too.

// This is a class of if I ever need to add more undo info, it will be put here
class UndoInfo {
    public:
    // If you want the raw value.
    std::optional<EditStorageItem> undone;
    bool success = false;

    UndoInfo (std::optional<EditStorageItem> t_undone);

    bool wasSuccess ();
};
using RedoInfo = UndoInfo;

using FilePositionStart = FilePosition;
using FilePositionEnd = FilePosition;
using ChunkSize = FilePosition;
using ChunkID = size_t;

class Chunk {
    public:
    FilePositionStart start;
    /// NOTE: data.size() may be less than size! size is the blocksize we were divvying up.
    ChunkSize size;
    Buffer data;
    /// How many times this chunk has read.
    size_t touched = 0;
    /// The time of the last time it was touched. 0 means this hasn't been set, or it's been reset due to error.
    std::chrono::milliseconds last_touched = std::chrono::milliseconds(0);

    Chunk (FilePositionStart t_start, ChunkSize t_size, Buffer t_data);
    Chunk (FilePositionStart t_start, ChunkSize t_size);

    bool isSizeEqual () const;
    void touch (size_t times=1);
    std::optional<std::chrono::milliseconds> timeElapsed ();
    size_t getRealSize () const;

    void updateTime ();
};

class Herix {
    protected:

    // TODO: think about moving the chunk data into it's own class (perhaps inside of this class via nested classes)

    /// Counts the current id for chunks
    ChunkID c_count = 0;
    /// Unique id to chunk to keep track
    /// I use this instead of a vector since I didn't want to deal with constantly modifying the
    /// current index when a chunk is removed from the vector.
    std::map<ChunkID, Chunk> chunks;

    /// The max memory that can be taken by chunks. Note that this isn't overall, just the chunk storage.
    /// If you want overall control, you will likely have to proactively mess with EditStorage and such.
    ChunkSize max_chunk_memory;
    /// Max chunk size.
    /// More chunks = more reading the file for parts, but also possibly less memory usage
    /// I rec having at least max_chunk
    ChunkSize chunk_size;

    // I would have this use Byte instead of char, but read fails (sets fail and bad bit) if you use Byte.
    // Since Byte=uint8_t=unsigned char, and the supported char_traits has char (and a few wchar things)
    // it will fail. I could write my own char_traits for unsigned char, but I'll just cast from char

    // No need to be wrapped in an optional since it can just have a file.
    std::fstream file;

    void destroyChunk (ChunkID id);
    void loadChunk (FilePosition pos);
    ChunkID getNewChunkID ();

    /// Swapping is used to note that we're swapping file (such as with saveas)
    /// And should be considered to be the same, so don't clear anything
    void openFile (bool swapping);

    public:

    // This should stay visible to code so they can directly mess with it if required.
    EditStorage edits;

    // TODO: move this into protected and make getter functions and ways to change the current file.
    std::filesystem::path filename;

    /// The filename to open
    /// Ten kilobytes. This default will likely be increased once the program is in a more stable state.
    /// Having at least three chunks being inside max_chunk memory is probably the best since it will allow more buffering to hide file loading.
    Herix (std::filesystem::path t_filename, ChunkSize t_max_chunk_memory=1024*10, ChunkSize t_chunk_size=1024);
    Herix (ChunkSize t_max_chunk_memory=1024*10, ChunkSize t_chunk_size=1024);

    bool hasFile () const;
    void loadFile (std::filesystem::path t_filename);
    void closeFile ();
    size_t getFileSize () const;

    size_t getChunkCount () const;
    bool hasChunks () const;
    bool hasChunk (ChunkID id) const;

    void cleanupChunks ();
    void invalidateChunks ();


    std::optional<Byte> read (FilePosition pos);
    std::optional<Byte> readRaw (FilePosition pos);
    // TODO: readMultiple and readMultipleRaw

    void edit (FilePosition pos, Byte value);
    void editMultiple (FilePosition pos, Buffer values);

    FilePosition getAlignedChunk (FilePosition pos) const;
    FilePosition getNearestAlignedChunk (FilePosition pos) const;

    std::optional<ChunkID> findChunk(FilePosition pos) const;

    UndoInfo undo ();
    RedoInfo redo ();

    bool hasUnsavedEdits () const;

    bool canUndo () const;
    bool canRedo () const;

    void saveHistoryDestructive ();
    void saveAsHistoryDestructive (std::string output);

    // TODO: find function that takes a position and bytes and returns the first one it finds, can then be sequentially tried
    // TODO: save function that doesn't destroy the history
    // - You will have to have a property which says it was written.
    // - There will have to be a way to undo the values other than just removing them.
    // - You'll have to keep what the previous value was or something.
    // TODO: save as function. Copy the file, then write to it
    // - Just make a generic save function that takes a filename, and save just calls it with current filename
    // TODO: function get nearest chunk, that does not have to include pos
};

#endif
