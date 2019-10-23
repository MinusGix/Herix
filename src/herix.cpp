#include <cassert>
#include <limits>

#include <iostream>
#include <algorithm>
#include <deque>

#include "herix.hpp"

using namespace HerixLib;

std::chrono::milliseconds getMillisecondsSinceEpoch ();




std::chrono::milliseconds getMillisecondsSinceEpoch () {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    );
}


UndoInfo::UndoInfo (std::optional<EditStorageItem> t_undone) {
    undone = t_undone;
    success = undone.has_value();
}

bool UndoInfo::wasSuccess () {
    return success;
}


Chunk::Chunk (FilePositionStart t_start, ChunkSize t_size, Buffer t_data) {
    start = t_start;
    size = t_size;
    data = t_data;
}
/// For construction which fills the vector in the struct
Chunk::Chunk (FilePositionStart t_start, ChunkSize t_size) {
    start = t_start;
    size = t_size;
}

bool Chunk::isSizeEqual () const {
    return size == data.size();
}

void Chunk::touch (size_t times) {
    touched += times;
    updateTime();
}

void Chunk::updateTime () {
    last_touched = getMillisecondsSinceEpoch();
}

size_t Chunk::getRealSize () const {
    return data.size();
}

std::optional<std::chrono::milliseconds> Chunk::timeElapsed () {
    if (last_touched == std::chrono::milliseconds(0)) {
        return std::nullopt;
    }

    return getMillisecondsSinceEpoch() - last_touched;
}


Herix::Herix (std::filesystem::path t_filename, bool t_allow_writing, AbsoluteFilePosition t_start_position, ChunkSize t_max_chunk_memory, ChunkSize t_chunk_size) {
    allow_writing = t_allow_writing;
    start_position = t_start_position;
    max_chunk_memory = t_max_chunk_memory;
    chunk_size = t_chunk_size;

    loadFile(t_filename);
}
Herix::Herix (bool t_allow_writing, AbsoluteFilePosition t_start_position, ChunkSize t_max_chunk_memory, ChunkSize t_chunk_size) {
    allow_writing = t_allow_writing;
    start_position = t_start_position;
    max_chunk_memory = t_max_chunk_memory;
    chunk_size = t_chunk_size;
}

AbsoluteFilePosition Herix::getStartPosition () const noexcept {
    return start_position;
}

bool Herix::hasFile () const {
    return file.is_open();
}

void Herix::loadFile (std::filesystem::path t_filename) {
    // We have a file. Close it up.
    if (hasFile()) {
        closeFile();
    }

    filename = t_filename;

    openFile(false);
}
// Does not currently use swapping, but it's there if we do strange things
void Herix::openFile (bool) {
    std::ios_base::openmode mode = std::ios_base::in | std::ios_base::binary;
    if (allow_writing) {
        mode |= std::ios_base::out;
    }

    file.open(filename, mode);

    if (file.fail()) {
        // TODO: make an errortype for this
        throw std::runtime_error("Failed in opening file.");
    }

    assert(file.is_open());

    file.unsetf(std::ios::skipws);
}

/// Closes file and throws away all data. Does NOT save any edits.
void Herix::closeFile () {
    if (file.is_open()) {
        file.clear(); // clear err state

        file.close();

        // Closing can have issues. If so, we don't know what to do, so complain.
        // Program using this library can catch it, and perhaps back up the edits and later restore them
        if (file.fail()) {
            // TODO: make an error type for this
            throw std::runtime_error("Failed in closing file.");
        }
    }

    edits.clear();
    chunks.clear();
    filename = "";
}

size_t Herix::getFileSize () const {
    return std::filesystem::file_size(filename);
}

size_t Herix::getFileEnd () const {
    size_t file_size = getFileSize();

    if (file_size <= start_position) {
        return 0;
    } else {
        return getFileSize() - start_position;
    }
}

void Herix::loadIntoChunk (FilePosition pos, ChunkSize read_size, ChunkID cid, Chunk& chunk, bool eof_handling) {
    // Modified pos
    size_t mpos = getStartPosition() + pos;

    // TODO: should we fail if mpos is past getFileEnd()?

    file.clear();
    file.seekg(static_cast<std::streamoff>(mpos));
    if (file.fail()) {
        throw std::runtime_error("Failed to seek to position in file!: " + std::to_string(mpos) + " | " + std::to_string(static_cast<std::streamoff>(mpos)));
    }

    chunk.data.resize(read_size);

    // Make sure there's enough space, there should be unless I'm misunderstanding
    assert(chunk.data.capacity() >= read_size);
    assert(read_size <= static_cast<ChunkSize>(std::numeric_limits<std::streamsize>::max()));

    file.clear();
    // Characters are extracted and stored until:
    // read_size characters were extracted and stored
    // or, EOF condition occurs on the input sequence. setstate(failbit|eofbit) is called.
    //    the number of extracted cahracters can be queried using gcount
    // TODO: find a way around using reinterpret cast :|
    file.read(reinterpret_cast<char*>(chunk.data.data()), static_cast<std::streamsize>(read_size));

    if (file.fail() && !file.eof()) { // fail, but not eof
        chunks.erase(cid);
        // I don't see anything about it being able to fail and not be in eof on my reference, but just in case
        throw std::runtime_error("Failed to read data from file!");
    } else if (file.fail() && file.eof()) {
        if (eof_handling) {
            // We are already handling eof.. so let's not continue.
            throw std::runtime_error("Eof handling failed! Previous size: " + std::to_string(read_size) + ", attempted current: " + std::to_string(file.gcount()));
        }
        loadIntoChunk(pos, file.gcount(), cid, chunk, true);
    } else {
        std::streamsize gcount = file.gcount();
        chunk.data.resize(gcount);
        // Might as well shrink
        chunk.data.shrink_to_fit();
    }
}

// We don't modify the pos here with the start_position since we're storing the data
void Herix::loadChunk (FilePositionStart pos, ChunkSize read_size) {
    if (!file.is_open()) {
        throw std::runtime_error("Attempting to load chunk whilst file was not open");
    }

    if (findChunk(pos).has_value()) {
        throw std::runtime_error("Attempted to load chunk that is already partially loaded!");
    }

    // Half-formed. This has to be filled in.
    //ChunkID cid = addChunk(Chunk(pos, chunk_size));
    ChunkID cid = getNewChunkID();
    chunks.emplace(std::make_pair(cid, Chunk(pos, chunk_size)));

    Chunk& chunk = chunks.at(cid);

    loadIntoChunk(pos, read_size, cid, chunk);
}

ChunkID Herix::getNewChunkID () {
    return c_count++; // return it's current value, then add 1
}


size_t Herix::getChunkCount () const {
    return chunks.size();
}
bool Herix::hasChunks () const {
    return chunks.size() > 0;
}

bool Herix::hasChunk (size_t id) const {
    return id < chunks.size();
}

// Throws away all the chunks.
void Herix::invalidateChunks () {
    chunks.clear();
}

/// Cleanup the chunks if they've gone over the limit.
/// Tries disposing of them in least used order and least recently loaded
void Herix::cleanupChunks (std::vector<ChunkID> ignore) {
    // We want to clean up in order of farthest away last used, and least used
    // Having the time it was last used lets us keep recently loaded chunks, and dump chunks that were loaded

    if (chunks.size() * chunk_size <= max_chunk_memory) {
        return;
    }

    std::deque<ChunkID> chunks_list;

    for (const std::pair<const ChunkID, Chunk>& c : chunks) {
        chunks_list.push_back(c.first);
    }

    // TODO: replace this with something better.
    // I add the .touched due to it being useful for dealing with events happening at the same milliseconds
    // It would be nice to give it more weight though
    std::sort(chunks_list.begin(), chunks_list.end(), [&](ChunkID a, ChunkID b) {
        Chunk& ac = chunks.at(a);
        Chunk& bc = chunks.at(b);
        return ac.last_touched != std::chrono::milliseconds(0) &&
            (ac.last_touched+std::chrono::milliseconds(ac.touched)) < (bc.last_touched+std::chrono::milliseconds(bc.touched));
    });
    chunks_list.erase(std::remove_if(
        chunks_list.begin(), chunks_list.end(),
        [&ignore](ChunkID cid) {
            for (ChunkID iid : ignore) {
                if (iid == cid) {
                    return true;
                }
            }
            return false;
        }), chunks_list.end());

    // We might have to cleanup multiple chunks since they may go over the limit.
    while (chunks_list.size() * chunk_size > max_chunk_memory) {
        if (chunks_list.size() == 0) {
            return;
        }

        ChunkID id = chunks_list.at(0);
        chunks_list.pop_front();

        chunks.erase(id);
    }
}

void Herix::destroyChunk (ChunkID id) {
    if (!hasChunk(id)) {
        throw std::invalid_argument("chunk argument did not point to an existing chunk to destroy.");
    }

    chunks.erase(id);
}

/// Returns the aligned chunk that includes the pos
FilePosition Herix::getAlignedChunk (FilePosition pos) const {
    assert((pos % chunk_size) <= pos);
    assert((pos - (pos % chunk_size)) % chunk_size == 0);
    return pos - (pos % chunk_size);
}

std::optional<ChunkID> Herix::findChunk(FilePosition pos) const {
    // TODO: make sure this is doing what you think (aka not copying the Chunk!)
    for (const std::pair<const ChunkID, Chunk>& chunk : chunks) {

        // TODO: check this logic to make sure it's not off by one
        // TODO: think if I should instead use the data size, since we aren't intending to support modification of file mid-edit
        if (pos >= chunk.second.start && pos < (chunk.second.start + chunk.second.size)) {
            return chunk.first;
        }
    }
    return std::nullopt;
}

/// Reads the value as it is stored in the file, loading the chunk if need be. Does not look at edits.
std::optional<Byte> Herix::readRaw (FilePosition pos) {
    std::optional<ChunkID> cid = findChunk(pos);

    if (!cid.has_value()) {
        try {
            loadChunk(getAlignedChunk(pos), chunk_size);
        } catch (...) {
            throw;
            //return std::nullopt;
        }

        cid = findChunk(pos);

        // If it still doesn't have a value and there wasn't an error, something weird is happening
        if (!cid.has_value()) {
            // TODO: remove this debug
            assert(false);
            return std::nullopt;
        }

        cleanupChunks({ cid.value() });
    }

    assert(cid.has_value());

    Chunk& chunk = chunks.at(cid.value());

    assert(pos >= chunk.start);

    chunk.touch();

    // It's valid for it to be out of range, since this Chunk might be on the edge
    // So it tries accessing something within the chunks realm but isn't actually existant
    if (chunk.data.size() <= pos - chunk.start) {
        return std::nullopt;
    }

    return chunk.data.at(pos - chunk.start);
}

/// Reads the value at that position, returning the edited value, falling back to files value, otherwise it is nullopt
/// Loads chunk if need be.
std::optional<Byte> Herix::read (FilePosition pos) {
    // Check editstorage first
    std::optional<Byte> stored_edit = edits.read(pos);
    if (stored_edit.has_value()) {
        return stored_edit;
    }

    return readRaw(pos);
}

std::vector<std::optional<Byte>> Herix::readMultipleRaw (FilePosition pos, size_t size) {
    std::vector<std::optional<Byte>> result;
    result.reserve(size);

    for (size_t i = 0; i < size; i++) {
        result.push_back(readRaw(pos + i));
    }

    return result;
}

std::vector<std::optional<Byte>> Herix::readMultiple (FilePosition pos, size_t size) {
    std::vector<std::optional<Byte>> result;
    result.reserve(size);

    for (size_t i = 0; i < size; i++) {
        result.push_back(read(pos + i));
    }

    return result;
}

std::vector<Byte> Herix::readMultipleCutoff (FilePosition pos, size_t size) {
    std::vector<Byte> result;
    // We're often going to be reading a large area from the file that isn't at the end
    // So I think this is appropriate.
    result.reserve(size);

    for (size_t i = 0; i < size; i++) {
        std::optional<Byte> byte = read(pos + i);
        if (!byte.has_value()) {
            break;
        }

        result.push_back(byte.value());
    }

    return result;
}

void Herix::edit (FilePosition pos, Byte value) {
    edits.edit(pos, value);
}

void Herix::editMultiple (FilePosition pos, Buffer values) {
    edits.editMultiple(pos, values);
}

/// Saves the files, just writes the edits and throws them away.
void Herix::saveHistoryDestructive () {
    if (!allow_writing) {
        return;
    }

    // TODO: make this efficient, so it only writes what it needs to
    // TODO: it'd also be nice to make so if it fails at writing, then there won't be partial writes
    for (const EditStorageItem& edit : edits.edits) {
        file.seekp(static_cast<std::streampos>(getStartPosition() + edit.pos));
        // TODO: this is icky, we need to replace file's type with one which uses Byte.
        // TODO: check if size is withing std::streamsize
        file.write(reinterpret_cast<const char *>(edit.data.data()), static_cast<std::streamsize>(edit.data.size()));
    }

    invalidateChunks();
    edits.clearNotStats();
}
/// Saves the files, to the filename. Overwrites if it already exists
/// It's up to the code using this to check if it already exists, if they care about that.
/// The current file then becomes output
// TODO: Make a function that only saves the portion of the file we're editing (getStartPosition() through getEndPosition)
void Herix::saveAsHistoryDestructive (std::string output) {
    if (!hasFile()) {
        throw std::runtime_error("No file.");
    }

    // We allow saving-as, even if allow_writing is false, since it's to a new file.

    try {
        std::filesystem::copy_file(filename, output);
    } catch (std::filesystem::filesystem_error&) {
        // We'll ignore it, because it throws errors if it already exists.
        // TODO: it'd be nice to ignore saving over files that exist but not ignore other errors
    }

    // Set the current file to output
    filename = output;
    // We're swapping over to output, so we have to close the original
    file.close();

    openFile(true);

    saveHistoryDestructive();
}

// = Undo/Redo

UndoInfo Herix::undo () {
    return UndoInfo(edits.undoR());
}
RedoInfo Herix::redo () {
    return RedoInfo(edits.redoR());
}

bool Herix::hasUnsavedEdits () const {
    // FIXME: this needs to be changed once there's ways of saving other than
    return canUndo();
}

bool Herix::canUndo () const {
    return edits.canUndo();
}

bool Herix::canRedo () const {
    return edits.canRedo();
}
