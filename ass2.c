/*
 *	Copyright (C) 2017  Hannes Haberl
 *	                    Matthias Tamegger
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OPTION_COUNT 2
#define MAP_MALLOC_INTERVALL 64
#define FILE_BUFFER_SIZE 128

#define ERR_INVALID_ARGUMENTS 1
#define ERR_OUT_OF_MEMORY 2
#define ERR_IO 3

// Only needed for the game graph analysis
typedef enum _GraphNodeStatus_
{
  DEAD_END = -1,    // Node is adjacent to two currently processing nodes,
  // this state exists to decrease recursion
  UNVISITED = 0,    // Node is not processed yet
  PROCESSING = 1,   // Node is currently being processed
  LEADS_TO_END = 2  // Node was visited and leads to an end
} GraphNodeStatus;

typedef struct _Chapter_
{
  char *title_;
  char *text_;
  struct _Chapter_ *options_[OPTION_COUNT];

  // Needed for the game graph analysis
  GraphNodeStatus graph_analyze_state_;
} Chapter;

typedef struct _MapEntry_
{
  char *key_;
  Chapter *value_;
} MapEntry;

typedef struct _Map_
{
  size_t length_;
  size_t count_;
  MapEntry *start_entry_;
} Map;


typedef struct list
{
  int abc;
  struct list *cdf;
} CoolList;

void initializeMap(Map *, int *);

size_t createMapEntryArray(MapEntry **, size_t, int *);

size_t resizeMap(Map *, size_t, int *);

Chapter *getChapterFromMap(Map *, char *);

Chapter *insertChapterIntoMap(Map *, char *, Chapter *, int *);

void freeMap(Map *);

void createChapter(Chapter **, int *);

int areEqual(Chapter *, Chapter *);

void loadAndAssignOptions(Chapter *, char *[OPTION_COUNT], Map *, int *);

void validateOptions(char *[OPTION_COUNT], int *);

void freeChapter(Chapter *);

void freeEntry(MapEntry *);

size_t createCharArray(char **, size_t, int *);

void getChapterPropertiesFromText(char *, char **, char **,
                                  char *[OPTION_COUNT], int *);

void loadChapterText(char *, char **, int *);

void readFile(FILE *, char **, int *);

void findAndReplaceNewLine(char **, int *);

int isEndOption(char *);

int isOptionValid(char *);

void analyzeGameGraph(Map *map, int *error);

void startGame(Chapter *);

int playChapter(Chapter **);

int getChoice();

void initializeWithFile(char *, Map *, Chapter **, int *);

void loadChapterFromFile(char *, Map *, Chapter **, int *);

void printError(int, char *);

//------------------------------------------------------------------------------
///
/// This is a small Text Adventure using a file structure as level design.
///
/// @return ERR_INVALID_ARGUMENTS 1, ERR_OUT_OF_MEMORY 2, ERR_IO 3 or if no
/// error occurred 0.
//
int main(int argc, char *argv[])
{
  // Basic argument validation
  if (argc != 2)
  {
    printError(ERR_INVALID_ARGUMENTS, NULL);
    return ERR_INVALID_ARGUMENTS;
  }
  char *start_file = argv[1];

  // Initialize
  Chapter *start_chapter = NULL;
  Map options_map = {
      .length_ = MAP_MALLOC_INTERVALL,
      .count_ = 0
  };
  int error = 0;
  initializeWithFile(start_file, &options_map, &start_chapter, &error);
  analyzeGameGraph(&options_map, &error);
  if (!error)
  {
    startGame(start_chapter);
  }
  freeMap(&options_map);
  printError(error, NULL);
  return error;
}

//-----------------------------------------------------------------------------
///
/// Initializes the Game.
/// Initializing the options_map and start loading the Chapters.
///
///
/// @param filename The filename of the first chapter.
/// @param options_map The Map into which all Chapter will be put.
/// @param start_chapter A reference to the pointer of the first chapter.
/// Will be created in the method.
/// @param error The error pointer that will be set if an error occurs.
///
/// @return nothing
//
void initializeWithFile(char *filename, Map *options_map,
                        Chapter **start_chapter, int *error)
{
  initializeMap(options_map, error);
  int a = 0b000101010;
  loadChapterFromFile(filename, options_map, start_chapter, error);
}

//-----------------------------------------------------------------------------
///
/// Starts the game with start_chapter.
/// Prints "ENDE" if the game was successfully finished.
///
///
/// @param start_chapter The Chapter with which the game will start.
///
/// @return nothing
//
void startGame(Chapter *start_chapter)
{
  Chapter *next_chapter = start_chapter;
  do
  {
    if (playChapter(&next_chapter))
    {
      return;
    }
  } while (next_chapter);
  printf("ENDE\n");
}

//-----------------------------------------------------------------------------
///
/// Prints the Chapter to stdout and takes user input for the Chapter.
/// If a new chapter is selected by the user, the reference chapter will be
/// updated to reference to the new chapter.
///
/// If an EOF was read as user inupt, EOF will be returned.
///
/// @param chapter A reference to the pointer of a chapter.
///
/// @return 0 or EOF if an EOF occured.
//
int playChapter(Chapter **chapter)
{
  printf("------------------------------\n");
  printf("%s\n\n%s\n\n", (*chapter)->title_, (*chapter)->text_);
  if ((*chapter)->options_[0] == NULL)
  {
    *chapter = NULL;
    return 0;
  }

  printf("Deine Wahl (A/B)? ");
  do
  {
    int choice = getChoice();
    if (choice < 0)
    {
      if (choice == EOF)
      {
        return EOF;
      }
      printf("[ERR] Please enter A or B.\n");
      continue;
    }
    *chapter = (*chapter)->options_[choice];
    return 0;
  } while (1);
}

//-----------------------------------------------------------------------------
///
/// Reads the choice (A/B) from stdin, and returns it's index(starting at 0).
/// If an EOF was read, the method returns EOF. For other errors a negative
/// number is returned.
///
/// @return The choice index or a negative number if an wrong input was done.
//
int getChoice()
{
  enum _State_
  {
    S_INVALID = -3, S_BEGIN = -2, S_CHOICE_A = 0, S_CHOICE_B = 1
  } state;
  state = S_BEGIN;

  while (1)
  {
    int input_character = getchar();
    if (input_character < 0)
    {
      return EOF;
    }
    if (input_character == '\n')
    {
      return state;
    }
    if (state == S_BEGIN)
    {
      switch (input_character)
      {
        case 'A':
          state = S_CHOICE_A;
          break;
        case 'B':
          state = S_CHOICE_B;
          break;
        default:
          state = S_INVALID;
      }
    }
    else
    {
      state = S_INVALID;
    }
  }
}

//-----------------------------------------------------------------------------
///
/// Loads a Chapter and all SubChapter/Options  from a file and puts them into
/// the options map.
///
///
/// @param filename The file from which the Chapter should be loaded.
/// @param options_map The Map into which all Chapter will be put.
/// @param chapter A reference to the pointer of the first Chapter. Will be set
/// in the method.
/// @param error The error pointer that will be set if an error occurs.
///
/// @return nothing
//
void loadChapterFromFile(char *filename, Map *options_map, Chapter **chapter,
                         int *error)
{
  if (*error)
  {
    return;
  }

  char *raw_chapter = NULL;
  loadChapterText(filename, &raw_chapter, error);
  createChapter(chapter, error);
  char *title = NULL;
  char *text = NULL;
  char *option_files[OPTION_COUNT];
  getChapterPropertiesFromText(raw_chapter, &title, &text, option_files, error);

  if (*chapter)
  {
    (*chapter)->title_ = title;
    (*chapter)->text_ = text;
  }

  validateOptions(option_files, error);
  if (*error == ERR_IO)
  {
    freeChapter(*chapter);
    printError(*error, filename);
    if (filename)
    {
      free(filename);
    }
    return;
  }


  Chapter *chapter_in_map = insertChapterIntoMap(options_map, filename,
                                                 *chapter, error);
  // If an duplicate is found we free the Chapter and we do not have
  // to assign the options again, as they are already set.
  if (chapter_in_map != *chapter)
  {
    freeChapter(*chapter);
    *chapter = chapter_in_map;
  }
  else
  {
    loadAndAssignOptions(*chapter, option_files, options_map, error);
  }
}

//-----------------------------------------------------------------------------
///
/// Loads the files/chapter of option_files if needed, and assignes the pointer
/// of the subchapter to chapter.
///
///
/// @param chapter A pointer to the Chapter on which the options should be set.
/// @param option_files The options that should be loaded. The options must be
/// validated already.
/// @param options_map The Map containing all already loaded Chapter.
/// @param error The error pointer that will be set if an error occurs.
///
/// @return nothing
//
void loadAndAssignOptions(Chapter *chapter, char *option_files[OPTION_COUNT],
                          Map *options_map, int *error)
{
  for (int option_index = 0;
       option_index < OPTION_COUNT && !*error;
       option_index++)
  {
    if (isEndOption(option_files[option_index]))
    {
      chapter->options_[option_index] = NULL;
      continue;
    }

    Chapter *subchapter = getChapterFromMap(options_map,
                                            option_files[option_index]);
    if (subchapter == NULL)
    {
      loadChapterFromFile(option_files[option_index], options_map, &subchapter,
                          error);
    }
    chapter->options_[option_index] = subchapter;
  }
}

//-----------------------------------------------------------------------------
///
/// Allocates memory for a Chapter.
///
/// Sets error to ERR_OUT_OF_MEMORY, if allocation fails.
///
/// @param chapter The reference to a pointer on which the memory will be
/// accessible. Or NULL if an error occurs.
/// @param error The error pointer that will be set if an error occurs.
///
/// @return nothing
//
void createChapter(Chapter **chapter, int *error)
{
  if (*error)
  {
    return;
  }

  *chapter = (Chapter *) malloc(sizeof(Chapter));
  if (*chapter == NULL)
  {
    *error = ERR_OUT_OF_MEMORY;
    return;
  }
}

//-----------------------------------------------------------------------------
///
/// Extracts the properties pointer of raw_chapter and replaces '\n' with null
/// terminators, so the pointers are limited.
///
/// @param raw_chapter The raw chapter text, as from a file.
/// @param title A reference to the pointer of the title. Will be overwritten.
/// @param text A reference to the pointer of the text. Will be overwritten.
/// @param options An array that will be filled with the options that are
/// extracted.
/// @param error The error pointer that will be set if an error occurs.
///
/// @return nothing
//
void getChapterPropertiesFromText(char *raw_chapter, char **title, char **text,
                                  char *options[OPTION_COUNT], int *error)
{
  if (*error)
  {
    return;
  }

  *title = raw_chapter;

  char *next_field = raw_chapter;
  findAndReplaceNewLine(&next_field, error);
  for (int option_index = 0;
       option_index < OPTION_COUNT && !*error;
       option_index++)
  {
    (options)[option_index] = next_field;
    findAndReplaceNewLine(&next_field, error);
  }
  *text = next_field;
}

//-----------------------------------------------------------------------------
///
/// Validates the options in the options array.
/// If the options are not valid, error will be set to ERR_IO.
///
/// @param options An array containing options.
/// @param error The error pointer that will be set if an error occurs.
///
/// @return nothing
//
void validateOptions(char *options[OPTION_COUNT], int *error)
{
  if (*error)
  {
    return;
  }

  // Checking first option for validity
  if (!isOptionValid(options[0]))
  {
    *error = ERR_IO;
    return;
  }
  int is_end_chapter = isEndOption(options[0]);

  // Check remaining options for validity
  for (int option_index = 1; option_index < OPTION_COUNT; option_index++)
  {
    if (!isOptionValid(options[option_index]) ||
        isEndOption(options[option_index]) != is_end_chapter)
    {
      *error = ERR_IO;
      return;
    }
  }
}

//-----------------------------------------------------------------------------
///
/// Returns if option represents a valid option. E.g. if the option is not
/// empty.
///
/// @param option The option that should be checked.
///
/// @return If option represents a valid option. E.g. if the option is not
/// empty.
//
int isOptionValid(char *option)
{
  return strlen(option) > 0;
}

//-----------------------------------------------------------------------------
///
/// Returns if option represents an "End option" e.g. if it is equal to "-".
///
/// @param option The option that should be checked.
///
/// @return If option represents an "End option" e.g. if it is equal to "-".
//
int isEndOption(char *option)
{
  return strlen(option) == 1 && option[0] == '-';
}

//-----------------------------------------------------------------------------
///
/// Initializes a Map, with a default length of MAP_MALLOC_INTERVALL
///
/// @param map A pointer to the map, that will be initialized.
/// @param error The error pointer that will be set if an error occurs.
///
/// @return nothing
//
void initializeMap(Map *map, int *error)
{
  MapEntry *map_entry;
  size_t size = createMapEntryArray(&map_entry, MAP_MALLOC_INTERVALL, error);
  map->start_entry_ = map_entry;
  map->length_ = size;
}

//-----------------------------------------------------------------------------
///
/// Searches for an entry in the map with the key in filename.
///
///
/// @param map The map, from which the chapter should be received.
/// @param filename The filename for the Chapter e.g. The key in the map.
///
/// @return A pointer to the Chapter or null, if no entry was found.
//
Chapter *getChapterFromMap(Map *map, char *filename)
{
  for (MapEntry *entry = map->start_entry_;
       entry < map->start_entry_ +
               map->count_; entry++)
  {
    if (strcmp(entry->key_, filename) == 0)
    {
      return entry->value_;
    }
  }
  return NULL;
}

//-----------------------------------------------------------------------------
///
/// Returns an equal Chapter from map if one exists, else NULL.
///
/// @param map The Map in which a equal Chapter should be searched.
/// @param chapter The Chapter for which an equal Chapter should be found.
///
/// @return An equal Chapter from map if one exists, else NULL.
//
Chapter *getEqualChapter(Map *map, Chapter *chapter)
{
  for (MapEntry *entry = map->start_entry_;
       entry < map->start_entry_ + map->count_;
       entry++)
  {
    if (areEqual(entry->value_, chapter))
    {
      return entry->value_;
    }
  }
  return NULL;
}

//-----------------------------------------------------------------------------
///
/// Inserts the chapter into the map. If the map has not enough space, it will
/// be resized.
///
/// @param map A pointer to the map, in which chapter will be inserted.
/// @param filename The filename e.g. The key in the map.
/// @param chapter A pointer to the chapter, that will be inserted.
/// @param error The error pointer that will be set if an error occurs.
///
/// @return A pointer to the saved Chapter.
//
Chapter *insertChapterIntoMap(Map *map, char *filename, Chapter *chapter,
                              int *error)
{
  if (*error)
  {
    return NULL;
  }
  if (map->count_ >= map->length_)
  {
    if (resizeMap(map, MAP_MALLOC_INTERVALL, error) == 0)
    {
      return NULL;
    }
  }
  Chapter *duplicate_chapter = getEqualChapter(map, chapter);

  MapEntry *new_entry = (map->start_entry_ + map->count_);
  map->count_++;

  new_entry->key_ = filename;
  if (duplicate_chapter)
  {
    new_entry->value_ = duplicate_chapter;
  }
  else
  {
    new_entry->value_ = chapter;
  }
  return new_entry->value_;
}

//-----------------------------------------------------------------------------
///
/// Returns the smaller of the two inputs.
///
/// @param first The first size_t to compare.
/// @param second The second size_t to compare.
///
/// @return The smaller of the two size_t parameters.
//
size_t min(size_t first, size_t second)
{
  return first < second ? first : second;
}

//-----------------------------------------------------------------------------
///
/// Checks if the two given Chapter are equal.
/// Equal is defined with having the same memory content/file content.
///
/// @param chapter_a The first chapter to compare.
/// @param chapter_b The second chapter to compare.
///
/// @return 1 if the Chapters are equal, else 0.
//
int areEqual(Chapter *chapter_a, Chapter *chapter_b)
{
  size_t length_a = strlen(chapter_a->text_);
  size_t length_b = strlen(chapter_b->text_);
  size_t memory_length_a = ((chapter_a->text_ + length_a) - chapter_a->title_);
  size_t memory_length_b = ((chapter_b->text_ + length_b) - chapter_b->title_);
  size_t min_size = min(memory_length_a, memory_length_b);

  return length_a == length_b
         && !memcmp(chapter_a->title_, chapter_b->title_, min_size);
}

//-----------------------------------------------------------------------------
///
/// Resizes the map by size.
/// If the allocation fails, the size will be halfed until the size is 1 and
/// then returns 0 and sets the error to ERR_OUT_OF_MEMORY.
///
/// @param map The map that should be resized.
/// @param size The size by which the map should be longer.
/// @param error The error pointer that will be set if an error occurs.
///
/// @return nothing
//
size_t resizeMap(Map *map, size_t size, int *error)
{
  while (1)
  {
    MapEntry *temporary_map_entry =
        realloc(map->start_entry_, (map->length_ + size) * sizeof(MapEntry));
    if (temporary_map_entry != NULL)
    {
      map->start_entry_ = temporary_map_entry;
      return size;
    }
    if (size == 1)
    {
      *error = ERR_OUT_OF_MEMORY;
      return 0;
    }
    size /= 2;
  }
}

//-----------------------------------------------------------------------------
///
/// Allocates memory for a MapEntry array of size size.
/// If the allocation fails, the size will be halfed until the size is 1 and
/// then returns 0 and sets the error to ERR_OUT_OF_MEMORY.
///
/// @param array The reference to the pointer on which the created array should
/// be accessible.
/// @param size The initial size of the created array.
/// @param error The error pointer that will be set if an error occurs.
///
/// @return nothing
//
size_t createMapEntryArray(MapEntry **array, size_t size, int *error)
{
  while (1)
  {
    *array = (MapEntry *) malloc(size * sizeof(MapEntry));
    if (*array != NULL)
    {
      return size;
    }

    if (size == 1)
    {
      *error = ERR_OUT_OF_MEMORY;
      return 0;
    }
    size /= 2;
  }
}

//-----------------------------------------------------------------------------
///
/// Sets all Chapter pointers, that are equal to the one in comparison_entry to
/// NULL.
/// NOTE: Only subsequent entries in the map are checked.
///
/// @param map The map containing the MapEntries
/// @param comparison_entry The entry determining from where to start and which
/// pointer should be set.
///
/// @return nothing
//
void clearSameChapterPointer(Map *map, MapEntry *comparison_entry)
{
  for (MapEntry *entry = comparison_entry + 1;
       entry < map->start_entry_ + map->count_;
       entry++)
  {
    if (entry->value_ == comparison_entry->value_)
    {
      entry->value_ = NULL;
    }
  }
}

//-----------------------------------------------------------------------------
///
/// Frees the memory of the given Map.
///
/// @param chapter A Map* that should be freed. Can be NULL.
///
/// @return nothing
//
void freeMap(Map *options_map)
{
  if (!options_map->start_entry_)
  {
    return;
  }

  for (MapEntry *entry = options_map->start_entry_;
       entry < options_map->start_entry_ + options_map->count_;
       entry++)
  {
    clearSameChapterPointer(options_map, entry);
    freeEntry(entry);
  }
  // Free chapter list
  free(options_map->start_entry_);
}

//-----------------------------------------------------------------------------
///
/// Reads the file content of file and dynamically resize the file_buffer.
///
/// The error will be set to ERR_IO or ERR_OUT_OF_MEMORY if an error occurs.
///
/// @param file A pointer to an already opened file.
/// @param file_buffer A pointer to an already allocated char array.
/// @param error The error pointer that will be set if an error occurs.
///
/// @return nothing
//
void readFile(FILE *file, char **file_buffer, int *error)
{
  if (*error)
  {
    return;
  }

  size_t read = 0;
  do
  {
    read += fread(*file_buffer + read, 1, FILE_BUFFER_SIZE, file);
    if (ferror(file))
    {
      *error = ERR_IO;
      return;
    }
    if (feof(file))
    {
      char *temporary_file_buffer = (char *) realloc(*file_buffer, read + 1);
      if (temporary_file_buffer == NULL)
      {
        *error = ERR_OUT_OF_MEMORY;
        return;
      }
      // Set the null terminator for the string
      *(temporary_file_buffer + read) = '\0';
      *file_buffer = temporary_file_buffer;
      return;
    }
    char *temporary_file_buffer = (char *) realloc(*file_buffer,
                                                   read + FILE_BUFFER_SIZE);
    if (temporary_file_buffer == NULL)
    {
      *error = ERR_OUT_OF_MEMORY;
      return;
    }
    *file_buffer = temporary_file_buffer;
  } while (1);
}

// This would be a possibility to ensure every file is only read once, no matter
// if the given path is relative, absolute or a symlink.
// However, this would need the POSIX standard, and is not allowed for the
// Assignment.
////----------------------------------------------------------------------------
/////
///// Normalizes the filename e.g. converts it to the absolute path.
/////
///// @param filename The filename that should be normalized.
///// @param error The error pointer that will be set if an error occurs.
/////
///// @return The normalized filename.
////
//char *normalizeFilename(char *filename, int *error)
//{
//  if (*error)
//  {
//    return NULL;
//  }
//  char *absolute_path = realpath(filename, NULL);
//  if (absolute_path == NULL)
//  {
//    switch (errno)
//    {
//      case ENOMEM:
//        *error = ERR_OUT_OF_MEMORY;
//        break;
//      default:
//        *error = ERR_IO;
//    }
//    return NULL;
//  }
//  return absolute_path;
//}

//-----------------------------------------------------------------------------
///
/// Loads the file content and puts it onto text.
///
/// The error will be set to ERR_IO or ERR_OUT_OF_MEMORY if an error occurs.
///
/// @param filename The file from which the text should be loaded.
/// @param text The reference to the pointer on which the text will be
/// accessible.
/// @param error The error pointer that will be set if an error occurs.
///
/// @return nothing
//
void loadChapterText(char *filename, char **text, int *error)
{
  if (*error)
  {
    return;
  }

  FILE *file = fopen(filename, "r");
  if (file == NULL)
  {
    *error = ERR_IO;
    return;
  }

  createCharArray(text, FILE_BUFFER_SIZE, error);

  readFile(file, text, error);
  fclose(file);
  if (*error)
  {
    if (*text)
    {
      free(*text);
    }
    return;
  }
}

//-----------------------------------------------------------------------------
///
/// Allocates memory for a char array of size size.
/// If the allocation fails, the size will be halfed until the size is 1 and
/// then returns 0 and sets the error to ERR_OUT_OF_MEMORY.
///
/// @param array The reference to the pointer on which the created array should
/// be accessible.
/// @param size The initial size of the created array.
/// @param error The error pointer that will be set if an error occurs.
///
/// @return nothing
//
size_t createCharArray(char **array, size_t size, int *error)
{
  while (1)
  {
    *array = (char *) malloc(size);
    if (*array != NULL)
    {
      return size;
    }

    if (size == 1)
    {
      *error = ERR_OUT_OF_MEMORY;
      return 0;
    }
    size /= 2;
  }
}

//-----------------------------------------------------------------------------
///
/// Searches for the first '\n' in text and replaces it with a null terminator.
/// Sets the pointer in text to the character after the replaced position.
///
/// Sets error to ERR_IO if no '\n' was found, or if the position after the
/// replaced character is not in the text.
///
/// @param text The text in which the '\n' should be relaced with a null
/// terminator.
/// @param error The error pointer that will be set if an error occurs.
///
/// @return nothing
//
void findAndReplaceNewLine(char **text, int *error)
{
  if (*error)
  {
    return;
  }
  *text = strchr(*text, '\n');
  if (*text == NULL)
  {
    *error = ERR_IO;
    return;
  }
  **text = '\0';
  ++*text;
  if (*text == NULL)
  {
    *error = ERR_IO;
    return;
  }
}

//-----------------------------------------------------------------------------
///
/// Frees the memory of the given MapEntry.
///
/// @param chapter A MapEntry* that should be freed. Can be NULL.
///
/// @return nothing
//
void freeEntry(MapEntry *entry)
{
  if (!entry)
  {
    return;
  }
  freeChapter(entry->value_);
}

//-----------------------------------------------------------------------------
///
/// Frees the memory of the given Chapter.
///
/// @param chapter A Chapter* that should be freed. Can be NULL.
///
/// @return nothing
//
void freeChapter(Chapter *chapter)
{
  if (!chapter)
  {
    return;
  }

  if (chapter->title_)
  {
    free(chapter->title_);
  }
  free(chapter);
}

//-----------------------------------------------------------------------------
///
/// Prints an error message for the given error_code.
///
/// @param error_code The error code for which an error message should be
/// printed.
/// @param argument char argument for error code ERR_IO, to provide the file
/// for which the message should be printed. If NULL, the ERR_IO will not be
/// printed.
///
/// @return nothing
//
void printError(const int error_code, char *argument)
{
  switch (error_code)
  {
    case ERR_INVALID_ARGUMENTS:
      printf("Usage: ./ass2 [file-name]\n");
      break;
    case ERR_IO:
      if (argument)
      {
        printf("[ERR] Could not read file %s.\n", argument);
      }
      break;
    case ERR_OUT_OF_MEMORY:
      printf("[ERR] Out of memory.\n");
      break;
    default:
      return;
  }

}

/**
 *
 * Graph functions
 *
 */

typedef enum _GraphClass_
{
  POSSIBLE = 1,
  HAS_MAZE = 2,
  NO_END = 0
} GraphClass;

GraphNodeStatus evaluateGraphNode(Chapter *root);

void resetGraphState(Map *);

void traverseGraph(Chapter *);

void visitGraphChilds(Chapter *);

void visitRemainingChilds(Chapter *);

GraphClass getGraphClass(Map *);

//-----------------------------------------------------------------------------
///
/// Analyzes the Graph represented by the Chapter.
/// It will be analyzed if the Graph:
/// - Contains loops
///   - If the loop can be exited
/// - Has an end
///
/// @param map The map containing all Chapters/the Graph to analyze.
/// @param error The error pointer that will be set if an error occurs.
///
/// @return nothing
//
void analyzeGameGraph(Map *map, int *error)
{
  if (*error)
  {
    return;
  }

  resetGraphState(map);

  // Traverse graph and analyze each node
  Chapter *root = map->start_entry_->value_;
  traverseGraph(root);

  // Iterate through map and evaluate the current loaded adventure
  GraphClass result = getGraphClass(map);
  switch (result)
  {
    case NO_END:
      // This also implies that there is a circle
      printf("[INFO] The loaded adventure has no reachable end!\n");
      break;

    case HAS_MAZE:
      // This also implies that there is a circle, that you can't get out of.
      printf(
          "[INFO] The loaded adventure contains a path that leads to a maze,"
              " that can't be exited anymore!\n");
      break;

    default:
      break;
  }
}

//-----------------------------------------------------------------------------
///
/// Resets the graph_analyze_state of every Chapter/graph node in the map.
///
/// @param map The map containing all Chapters/the Graph to analyze.
///
/// @return nothing
//
void resetGraphState(Map *map)
{
  for (MapEntry *entry = map->start_entry_;
       entry < map->start_entry_ + map->count_;
       entry++)
  {
    // init graph analysis state
    entry->value_->graph_analyze_state_ = UNVISITED;
  }
}

//-----------------------------------------------------------------------------
///
/// Checks the state of root and commence further actions depending on the
/// state:
/// - VISITED No Action needed
/// - DEAD_END No Action needed
/// - PROCESSING Start exploring the alternate childs
/// - UNVISITED Start exploring all childs
///
/// @param root The graph node/Chapter whichs sub tree should be analyzed.
///
/// @return nothing
//
void traverseGraph(Chapter *root)
{
  switch (root->graph_analyze_state_)
  {
    case LEADS_TO_END:
    case DEAD_END:
      // Skip node evaluation
      return;

    case PROCESSING:
      // Explore alternate paths on graph nodes
      visitRemainingChilds(root);
      break;

    case UNVISITED:
    default:
      // Normally process graph node
      root->graph_analyze_state_ = PROCESSING;
      visitGraphChilds(root);
  }
}

//-----------------------------------------------------------------------------
///
/// Visits all childs of root and either goes deeper down the graph, or sets
/// the state to VISITED, if the root has a null child.
///
/// NOTE: It is defined, that if one child is NULL, there are no other childs
/// in this root.
///
/// @param root The graph node/Chapter whichs childs should be analyzed.
///
/// @return nothing
//
void visitGraphChilds(Chapter *root)
{
  for (int current_path = 0; current_path < OPTION_COUNT; current_path++)
  {
    Chapter *child = root->options_[current_path];
    if (child != NULL)
    {
      // Recursively traverse graph
      traverseGraph(child);
    }
    else
    {
      // Node is an end node
      root->graph_analyze_state_ = LEADS_TO_END;
      return;
    }
  }
  // Evaluate node status
  root->graph_analyze_state_ = evaluateGraphNode(root);
}

//-----------------------------------------------------------------------------
///
/// Evaluates if root lead to an end or if it is a DEAD_END.
///
/// NOTE: During processing of the whole graph DEAD_END could be temporary.
///
/// @param root The graph node/Chapter which should be evaluated.
///
/// @return The freshly evaluated state for root.(LEADS_TO_END or DEAD_END)
//
GraphNodeStatus evaluateGraphNode(Chapter *root)
{
  for (int current_path = 0; current_path < OPTION_COUNT; current_path++)
  {
    Chapter *child = root->options_[current_path];
    // Node is connected to a node that leads to an end
    if (child->graph_analyze_state_ == LEADS_TO_END)
    {
      return LEADS_TO_END;
    }
  }
  // Mark node as dead end to reduce further recursion
  return DEAD_END;
}

//-----------------------------------------------------------------------------
///
/// Visits all childs of root and either goes deeper down the graph, or sets
/// the state to VISITED, if the root has a null child.
///
/// @param root The graph node/Chapter whichs childs should be analyzed.
///
/// @return nothing
//
void visitRemainingChilds(Chapter *root)
{
  for (int current_path = OPTION_COUNT - 1; current_path > 0; current_path--)
  {
    Chapter *child = root->options_[current_path];
    // Node is connected to a node that leads to an end
    if (!child || child->graph_analyze_state_ == LEADS_TO_END)
    {
      root->graph_analyze_state_ = LEADS_TO_END;
      return;
    }
    // Visit unvisited children
    if (child->graph_analyze_state_ == UNVISITED)
    {
      traverseGraph(child);
    }
  }
  // Evaluate node status
  root->graph_analyze_state_ = evaluateGraphNode(root);
}

//-----------------------------------------------------------------------------
///
/// Classifies a graph represented by a Chapter map according to it's
/// properties.
///
/// @param map The map containing the Chapter graph.
///
/// @return The GraphClass of the Graph in map
//
GraphClass getGraphClass(Map *map)
{
  Chapter *root = map->start_entry_->value_;
  // Root has no child Chapter that leads to an end, that implies that there
  // exists no reachable end in this adventure
  if (root->graph_analyze_state_ != LEADS_TO_END)
  {
    return NO_END;
  }

  // Iterate through map and look at every visited node, if a single not wasn't
  // visited that implies that there must be a inescapable maze
  for (MapEntry *entry = map->start_entry_;
       entry < map->start_entry_ +
               map->count_; entry++)
  {
    if (entry->value_->graph_analyze_state_ != LEADS_TO_END)
    {
      return HAS_MAZE;
    }
  }
  // If every chapter can reach an end, that means that the adventure is
  // possible to play, every circle has at least one possible way
  // that leads to an end
  return POSSIBLE;
}