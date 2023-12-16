// A SQLite clone written in C
// Based on cstack.github.io/db_tutorial/
// For learning purposes, extensive comments 

#include <errno.h>  // Used for error handling through error codes
#include <fcntl.h>  // File control options like open, read, write permissions
#include <stdbool.h>  // Provides a boolean data type and values true/false
#include <stdint.h>  // Fixed-width integers like int32_t, uint64_t, etc.
#include <stdio.h>  // Standard Input/Output operations like printf, scanf
#include <stdlib.h>  // General purpose standard library, includes memory allocation, process control, conversions, etc.
#include <string.h>  // String handling functions like strcpy, strlen, etc.
#include <unistd.h>  // Provides access to POSIX operating system API, includes read, write, close, etc.

/*
InputBuffer: A struct to manage the input buffer for user input.
*/

typedef struct {
  char* buffer;           // Pointer to the buffer holding the input string
  size_t buffer_length;   // The total capacity of the buffer
  ssize_t input_length;   // The actual length of the input string
} InputBuffer;

/*
ExecuteResult: An enumeration for the result of executing a SQL statement.
*/

typedef enum {
  EXECUTE_SUCCESS,        // Indicates successful execution of a statement
  EXECUTE_DUPLICATE_KEY,  // Indicates an execution failure due to a duplicate key
} ExecuteResult;

/*
MetaCommandResult: An enumeration for the result of interpreting a meta-command.
*/

typedef enum {
  META_COMMAND_SUCCESS,             // Indicates successful interpretation of a meta-command
  META_COMMAND_UNRECOGNIZED_COMMAND // Indicates failure due to an unrecognized meta-command
} MetaCommandResult;

/*
PrepareResult: An enumeration for the result of preparing a SQL statement.
*/

typedef enum {
  PREPARE_SUCCESS,               // Indicates successful preparation of a statement
  PREPARE_NEGATIVE_ID,           // Indicates an error due to a negative ID in an INSERT statement
  PREPARE_STRING_TOO_LONG,       // Indicates an error due to a string being too long
  PREPARE_SYNTAX_ERROR,          // Indicates a syntax error in the SQL statement
  PREPARE_UNRECOGNIZED_STATEMENT // Indicates an unrecognized statement type
} PrepareResult;

/*
StatementType: Enumeration for types of SQL statements.
*/

typedef enum { 
  STATEMENT_INSERT, // Represents an INSERT statement
  STATEMENT_SELECT  // Represents a SELECT statement
} StatementType;


// Define the maximum size for the username column
#define COLUMN_USERNAME_SIZE 32

// Define the maximum size for the email column
#define COLUMN_EMAIL_SIZE 255

/*
Row: A structure representing a row in the database.
*/

typedef struct {
  uint32_t id;                              // Unique identifier for the row
  char username[COLUMN_USERNAME_SIZE + 1];  // Username field with a fixed size
  char email[COLUMN_EMAIL_SIZE + 1];        // Email field with a fixed size
} Row;

/*
Statement: A structure representing a SQL statement.
*/

typedef struct {
  StatementType type;   // Type of the statement (e.g., INSERT, SELECT)
  Row row_to_insert;    // Row to be inserted, used only by INSERT statements
} Statement;

// Macro to determine the size of a specific attribute within a structure
#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)

// Defining some constants for the database
const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;
const uint32_t PAGE_SIZE = 4096;

#define TABLE_MAX_PAGES 400
#define INVALID_PAGE_NUM UINT32_MAX

/*
Pager: A structure to manage the pages of the database file.
*/

typedef struct {
  int file_descriptor;          // File descriptor for the database file.
  uint32_t file_length;         // Total length of the file in bytes.
  uint32_t num_pages;           // Number of pages currently in the file.
  void* pages[TABLE_MAX_PAGES]; // Array of pointers to the pages loaded into memory.
} Pager;

/*
Table: A structure representing a table in the database.
*/

typedef struct {
  Pager* pager;             // Pointer to the Pager managing this table's pages.
  uint32_t root_page_num;   // The page number of the root page in the B-tree.
} Table;

/*
Cursor: A structure for navigating through the table.
*/

typedef struct {
  Table* table;       // Pointer to the table to navigate.
  uint32_t page_num;  // Current page number in the table.
  uint32_t cell_num;  // Current cell number on the current page.
  bool end_of_table;  // Boolean flag indicating if the cursor is past the last element in the table.
} Cursor;



/**
 * Function to print a database row.
 * @param row Pointer to the Row structure to be printed.
 * @return void.
 */

void print_row(Row* row) {
  printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}

/*
NodeType: Enumeration representing the type of a node in a B-tree.
*/

typedef enum { 
  NODE_INTERNAL,  // Represents an internal node of the B-tree
  NODE_LEAF       // Represents a leaf node of the B-tree
  } NodeType;

/*
 * Common Node Header Layout
 */
const uint32_t NODE_TYPE_SIZE = sizeof(uint8_t);  // Size of the node type field
const uint32_t NODE_TYPE_OFFSET = 0;  // Offset of the node type field in the node
const uint32_t IS_ROOT_SIZE = sizeof(uint8_t);  // Size of the 'is root' field
const uint32_t IS_ROOT_OFFSET = NODE_TYPE_SIZE; // Offset of the 'is root' field in the node
const uint32_t PARENT_POINTER_SIZE = sizeof(uint32_t);  // Size of the parent pointer field
const uint32_t PARENT_POINTER_OFFSET = IS_ROOT_OFFSET + IS_ROOT_SIZE; // Offset of the parent pointer in the node
const uint8_t COMMON_NODE_HEADER_SIZE =
    NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE;  // Total size of the common node header

/*
 * Internal Node Header Layout
 */
const uint32_t INTERNAL_NODE_NUM_KEYS_SIZE = sizeof(uint32_t);  // Size of the 'number of keys' field
const uint32_t INTERNAL_NODE_NUM_KEYS_OFFSET = COMMON_NODE_HEADER_SIZE; // Offset of the 'number of keys' field
const uint32_t INTERNAL_NODE_RIGHT_CHILD_SIZE = sizeof(uint32_t); // Size of the right child pointer field
const uint32_t INTERNAL_NODE_RIGHT_CHILD_OFFSET =
    INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE;  // Offset of the right child pointer
const uint32_t INTERNAL_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE +
                                           INTERNAL_NODE_NUM_KEYS_SIZE +
                                           INTERNAL_NODE_RIGHT_CHILD_SIZE;  // Total size of the internal node header

/*
 * Internal Node Body Layout
 */
const uint32_t INTERNAL_NODE_KEY_SIZE = sizeof(uint32_t); // Size of the key field in an internal node
const uint32_t INTERNAL_NODE_CHILD_SIZE = sizeof(uint32_t); // Size of the child pointer field
const uint32_t INTERNAL_NODE_CELL_SIZE =
    INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE;  // Total size of a cell in an internal node
/* Keep this small for testing */
const uint32_t INTERNAL_NODE_MAX_KEYS = 3;  // Maximum number of keys in an internal node (for testing)

/*
 * Leaf Node Header Layout
 */
const uint32_t LEAF_NODE_NUM_CELLS_SIZE = sizeof(uint32_t); // Size of the 'number of cells' field
const uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;  // Offset of the 'number of cells' field
const uint32_t LEAF_NODE_NEXT_LEAF_SIZE = sizeof(uint32_t); // Size of the next leaf pointer field
const uint32_t LEAF_NODE_NEXT_LEAF_OFFSET =
    LEAF_NODE_NUM_CELLS_OFFSET + LEAF_NODE_NUM_CELLS_SIZE;  // Offset of the next leaf pointer
const uint32_t LEAF_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE +
                                       LEAF_NODE_NUM_CELLS_SIZE +
                                       LEAF_NODE_NEXT_LEAF_SIZE;  // Total size of the leaf node header

/*
 * Leaf Node Body Layout
 */
const uint32_t LEAF_NODE_KEY_SIZE = sizeof(uint32_t); // Size of the key field in a leaf node
const uint32_t LEAF_NODE_KEY_OFFSET = 0;   // Offset of the key field in a leaf node cell
const uint32_t LEAF_NODE_VALUE_SIZE = ROW_SIZE; // Size of the value field in a leaf node
const uint32_t LEAF_NODE_VALUE_OFFSET =
    LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE;  // Offset of the value field in a leaf node cell
const uint32_t LEAF_NODE_CELL_SIZE = LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE; // Total size of a cell in a leaf node
const uint32_t LEAF_NODE_SPACE_FOR_CELLS = PAGE_SIZE - LEAF_NODE_HEADER_SIZE; // Available space for cells in a leaf node
const uint32_t LEAF_NODE_MAX_CELLS =
    LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE;  // Maximum number of cells in a leaf node
const uint32_t LEAF_NODE_RIGHT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) / 2; // Number of cells to keep in the right node after splitting
const uint32_t LEAF_NODE_LEFT_SPLIT_COUNT =
    (LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT;  // Number of cells to keep in the left node after splitting

/**
 * Get the node type from a given node.
 * @param node Pointer to the node from which the type is to be retrieved.
 * @return NodeType of the node.
 */

NodeType get_node_type(void* node) {
  // Dereference the node type after offsetting by NODE_TYPE_OFFSET
  uint8_t value = *((uint8_t*)(node + NODE_TYPE_OFFSET));
  return (NodeType)value;
}

/**
 * Set the node type for a given node.
 * @param node Pointer to the node for which the type is to be set.
 * @param type NodeType value to set for the node.
 */

void set_node_type(void* node, NodeType type) {
  uint8_t value = type; // Cast NodeType to uint8_t
  // Set the node type after offsetting by NODE_TYPE_OFFSET
  *((uint8_t*)(node + NODE_TYPE_OFFSET)) = value;
}

/**
 * Check if a given node is the root of the tree.
 * @param node Pointer to the node to check.
 * @return True if the node is the root, false otherwise.
 */

bool is_node_root(void* node) {
  // Dereference the 'is root' value after offsetting by IS_ROOT_OFFSET
  uint8_t value = *((uint8_t*)(node + IS_ROOT_OFFSET));
  return (bool)value;
}

/**
 * Set the given node as a root or non-root node.
 * @param node Pointer to the node to set as root or non-root.
 * @param is_root Boolean indicating whether the node is a root node or not.
 */

void set_node_root(void* node, bool is_root) {
  uint8_t value = is_root; // Cast bool to uint8_t
// Set the 'is root' value after offsetting by IS_ROOT_OFFSET

  *((uint8_t*)(node + IS_ROOT_OFFSET)) = value;
}

/**
 * Retrieves the parent pointer from a given node.
 * @param node Pointer to the node from which to get the parent pointer.
 * @return Pointer to the parent pointer field in the node.
 */

uint32_t* node_parent(void* node) { 
  // Return the address of the parent pointer in the node
  return node + PARENT_POINTER_OFFSET; 
}

/**
 * Retrieves the number of keys in an internal node.
 * @param node Pointer to the internal node.
 * @return Pointer to the number of keys field in the node.
 */

uint32_t* internal_node_num_keys(void* node) {
  // Return the address of the number of keys field in the internal node
  return node + INTERNAL_NODE_NUM_KEYS_OFFSET;
}

/**
 * Retrieves the right child of an internal node.
 * @param node Pointer to the internal node.
 * @return Pointer to the right child field in the node.
 */

uint32_t* internal_node_right_child(void* node) {
  // Return the address of the right child field in the internal node
  return node + INTERNAL_NODE_RIGHT_CHILD_OFFSET;
}

/**
 * Retrieves a specific cell from an internal node.
 * @param node Pointer to the internal node.
 * @param cell_num Cell number to retrieve.
 * @return Pointer to the specified cell in the node.
 */

uint32_t* internal_node_cell(void* node, uint32_t cell_num) {
  // Return the address of the specified cell in the internal node
  return node + INTERNAL_NODE_HEADER_SIZE + cell_num * INTERNAL_NODE_CELL_SIZE;
}

/**
 * Retrieves a specific child pointer from an internal node.
 * @param node Pointer to the internal node.
 * @param child_num The index of the child to retrieve.
 * @return Pointer to the specified child in the node.
 */

uint32_t* internal_node_child(void* node, uint32_t child_num) {
  uint32_t num_keys = *internal_node_num_keys(node);
  if (child_num > num_keys) {
    // Error handling for accessing an out-of-bounds child

    printf("Tried to access child_num %d > num_keys %d\n", child_num, num_keys);
    exit(EXIT_FAILURE);
  } else if (child_num == num_keys) {
    // Handle case when child_num is equal to num_keys (accessing the rightmost child)
    uint32_t* right_child = internal_node_right_child(node);
    if (*right_child == INVALID_PAGE_NUM) {
      printf("Tried to access right child of node, but was invalid page\n");
      exit(EXIT_FAILURE);
    }
    return right_child;
  } else {
    // Normal case for accessing a child within the bounds
    uint32_t* child = internal_node_cell(node, child_num);
    if (*child == INVALID_PAGE_NUM) {
      printf("Tried to access child %d of node, but was invalid page\n", child_num);
      exit(EXIT_FAILURE);
    }
    return child;
  }
}

/**
 * Retrieves a specific key from an internal node.
 * @param node Pointer to the internal node.
 * @param key_num The index of the key to retrieve.
 * @return Pointer to the specified key in the node.
 */

uint32_t* internal_node_key(void* node, uint32_t key_num) {
  // Calculate and return the address of the specified key
  return (void*)internal_node_cell(node, key_num) + INTERNAL_NODE_CHILD_SIZE;
}

/**
 * Retrieves the number of cells in a leaf node.
 * @param node Pointer to the leaf node.
 * @return Pointer to the number of cells field in the leaf node.
 */

uint32_t* leaf_node_num_cells(void* node) {
  // Return the address of the number of cells field in the leaf node
  return (uint32_t*)(node + LEAF_NODE_NUM_CELLS_OFFSET);
}

/**
 * Retrieves the pointer to the next leaf node.
 * @param node Pointer to the current leaf node.
 * @return Pointer to the next leaf field in the node.
 */

uint32_t* leaf_node_next_leaf(void* node) {
  // Return the address of the next leaf pointer field in the leaf node
  return node + LEAF_NODE_NEXT_LEAF_OFFSET;
}

/**
 * Retrieves a specific cell from a leaf node.
 * @param node Pointer to the leaf node.
 * @param cell_num Cell number to retrieve.
 * @return Pointer to the specified cell in the leaf node.
 */

void* leaf_node_cell(void* node, uint32_t cell_num) {
  // Calculate and return the address of the specified cell in the leaf node
  return node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE;
}

/**
 * Retrieves the key at a specific cell in a leaf node.
 * @param node Pointer to the leaf node.
 * @param cell_num Cell number to retrieve the key from.
 * @return Pointer to the key in the specified cell.
 */

uint32_t* leaf_node_key(void* node, uint32_t cell_num) {
  // Return the address of the key in the specified cell
  return leaf_node_cell(node, cell_num);
}

/**
 * Retrieves the value at a specific cell in a leaf node.
 * @param node Pointer to the leaf node.
 * @param cell_num Cell number to retrieve the value from.
 * @return Pointer to the value in the specified cell.
 */

void* leaf_node_value(void* node, uint32_t cell_num) {
  // Calculate and return the address of the value in the specified cell
  return leaf_node_cell(node, cell_num) + LEAF_NODE_KEY_SIZE;
}

/**
 * Retrieves a specific page from the pager.
 * @param pager Pointer to the Pager structure.
 * @param page_num The page number to retrieve.
 * @return Pointer to the requested page.
 */

void* get_page(Pager* pager, uint32_t page_num) {
  // Check for out-of-bounds page number
  if (page_num > TABLE_MAX_PAGES) {
    printf("Tried to fetch page number out of bounds. %d > %d\n", page_num,
           TABLE_MAX_PAGES);
    exit(EXIT_FAILURE);
  }
  // Handle cache miss
  if (pager->pages[page_num] == NULL) {
    // Allocate memory for the page
    void* page = malloc(PAGE_SIZE);
    // Calculate the number of pages in the file
    uint32_t num_pages = pager->file_length / PAGE_SIZE;
    // Load the page from the file if it exists
    if (pager->file_length % PAGE_SIZE) {
      num_pages += 1;
    }

    if (page_num <= num_pages) {
      lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
      ssize_t bytes_read = read(pager->file_descriptor, page, PAGE_SIZE);
      if (bytes_read == -1) {
        printf("Error reading file: %d\n", errno);
        exit(EXIT_FAILURE);
      }
    }
    // Update the pager's pages array and page count
    pager->pages[page_num] = page;

    if (page_num >= pager->num_pages) {
      pager->num_pages = page_num + 1;
    }
  }
  // Return the requested page
  return pager->pages[page_num];
}

/**
 * Gets the maximum key from a node.
 * @param pager Pointer to the Pager structure.
 * @param node Pointer to the node.
 * @return The maximum key in the node.
 */

uint32_t get_node_max_key(Pager* pager, void* node) {
  // Handle leaf nodes
  if (get_node_type(node) == NODE_LEAF) {
    return *leaf_node_key(node, *leaf_node_num_cells(node) - 1);
  }
  // Recursively call on the right child for internal nodes
  void* right_child = get_page(pager,*internal_node_right_child(node));
  return get_node_max_key(pager, right_child);
}

/**
 * Prints constant values used in the database.
 */

void print_constants() {
  printf("ROW_SIZE: %d\n", ROW_SIZE);
  printf("COMMON_NODE_HEADER_SIZE: %d\n", COMMON_NODE_HEADER_SIZE);
  printf("LEAF_NODE_HEADER_SIZE: %d\n", LEAF_NODE_HEADER_SIZE);
  printf("LEAF_NODE_CELL_SIZE: %d\n", LEAF_NODE_CELL_SIZE);
  printf("LEAF_NODE_SPACE_FOR_CELLS: %d\n", LEAF_NODE_SPACE_FOR_CELLS);
  printf("LEAF_NODE_MAX_CELLS: %d\n", LEAF_NODE_MAX_CELLS);
}

/**
 * Indents the output based on the specified indentation level.
 * @param level The level of indentation.
 */

void indent(uint32_t level) {
  // Print spaces for indentation
  for (uint32_t i = 0; i < level; i++) {
    printf("  ");
  }
}

/**
 * Prints the B-tree starting from a specific page.
 * @param pager Pointer to the Pager structure.
 * @param page_num The starting page number of the B-tree to print.
 * @param indentation_level Current level of indentation for pretty printing.
 */

void print_tree(Pager* pager, uint32_t page_num, uint32_t indentation_level) {
  void* node = get_page(pager, page_num);
  uint32_t num_keys, child;
  // Handle different node types
  switch (get_node_type(node)) {
    case (NODE_LEAF):
      num_keys = *leaf_node_num_cells(node);
      indent(indentation_level);
      printf("- leaf (size %d)\n", num_keys);
      for (uint32_t i = 0; i < num_keys; i++) {
        indent(indentation_level + 1);
        printf("- %d\n", *leaf_node_key(node, i));
      }
      break;
    case (NODE_INTERNAL):
      num_keys = *internal_node_num_keys(node);
      indent(indentation_level);
      printf("- internal (size %d)\n", num_keys);
      // Print child nodes and keys
      if (num_keys > 0) {
        for (uint32_t i = 0; i < num_keys; i++) {
          child = *internal_node_child(node, i);
          print_tree(pager, child, indentation_level + 1);

          indent(indentation_level + 1);
          printf("- key %d\n", *internal_node_key(node, i));
        }
        // Handle the rightmost child
        child = *internal_node_right_child(node);
        print_tree(pager, child, indentation_level + 1);
      }
      break;
  }
}

/**
 * Serializes a Row structure into a byte array.
 * @param source Pointer to the Row structure to be serialized.
 * @param destination Pointer to the destination byte array where the serialized data will be stored.
 */

void serialize_row(Row* source, void* destination) {
  // Copy the ID from the source to the destination at the ID_OFFSET
  memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
  // Copy the username from the source to the destination at the USERNAME_OFFSET
  memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
  // Copy the email from the source to the destination at the EMAIL_OFFSET
  memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}

/**
 * Deserializes a byte array into a Row structure.
 * @param source Pointer to the byte array containing the serialized data.
 * @param destination Pointer to the Row structure where the deserialized data will be stored.
 */

void deserialize_row(void* source, Row* destination) {
  // Copy the ID from the source to the destination at the ID_OFFSET
  memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
  // Copy the username from the source to the destination at the USERNAME_OFFSET
  memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
  // Copy the email from the source to the destination at the EMAIL_OFFSET
  memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

/**
 * Initializes a node as a leaf node.
 * @param node Pointer to the node to be initialized.
 */

void initialize_leaf_node(void* node) {
  // Set the node type to NODE_LEAF
  set_node_type(node, NODE_LEAF);
  // Set the node as non-root
  set_node_root(node, false);
  // Initialize the number of cells to 0
  *leaf_node_num_cells(node) = 0;
  // Set the next leaf pointer to 0 (no sibling)
  *leaf_node_next_leaf(node) = 0;
}

/**
 * Initializes a node as an internal node.
 * @param node Pointer to the node to be initialized.
 */

void initialize_internal_node(void* node) {
  // Set the node type to NODE_INTERNAL
  set_node_type(node, NODE_INTERNAL);
  // Set the node as non-root
  set_node_root(node, false);
  // Initialize the number of keys to 0
  *internal_node_num_keys(node) = 0;
  /*
  Necessary because the root page number is 0; by not initializing an internal 
  node's right child to an invalid page number when initializing the node, we may
  end up with 0 as the node's right child, which makes the node a parent of the root
  */
  *internal_node_right_child(node) = INVALID_PAGE_NUM;
}

/**
 * Finds a particular key within a leaf node and returns a cursor pointing to it.
 * @param table Pointer to the Table structure.
 * @param page_num The page number of the leaf node.
 * @param key The key to find in the leaf node.
 * @return Cursor pointing to the location of the key, or where the key should be if it's not present.
 */

Cursor* leaf_node_find(Table* table, uint32_t page_num, uint32_t key) {
  void* node = get_page(table->pager, page_num);  // Retrieve the specified page
  uint32_t num_cells = *leaf_node_num_cells(node);  // Get the number of cells in the leaf node
  // Allocate and initialize a new cursor
  Cursor* cursor = malloc(sizeof(Cursor));
  cursor->table = table;
  cursor->page_num = page_num;
  cursor->end_of_table = false;

  // Binary search
  uint32_t min_index = 0;
  uint32_t one_past_max_index = num_cells;
  while (one_past_max_index != min_index) {
    uint32_t index = (min_index + one_past_max_index) / 2;
    uint32_t key_at_index = *leaf_node_key(node, index);
    if (key == key_at_index) {
      cursor->cell_num = index; // Key found
      return cursor;
    }
    if (key < key_at_index) {
      one_past_max_index = index;
    } else {
      min_index = index + 1;
    }
  }

  cursor->cell_num = min_index; // Key not found, return position where it should be inserted
  return cursor;
}

/**
 * Finds the child index in an internal node that should contain the given key.
 * @param node Pointer to the internal node.
 * @param key The key to find.
 * @return The index of the child that should contain the key.
 */

uint32_t internal_node_find_child(void* node, uint32_t key) {
  /*
  Return the index of the child which should contain
  the given key.
  */

  uint32_t num_keys = *internal_node_num_keys(node);

  /* Binary search */
  uint32_t min_index = 0;
  uint32_t max_index = num_keys; /* there is one more child than key */

  while (min_index != max_index) {
    uint32_t index = (min_index + max_index) / 2;
    uint32_t key_to_right = *internal_node_key(node, index);
    if (key_to_right >= key) {
      max_index = index;
    } else {
      min_index = index + 1;
    }
  }

  return min_index; // Return the index of the child that should contain the key
}

/**
 * Finds a key within an internal node and returns a cursor pointing to it.
 * @param table Pointer to the Table structure.
 * @param page_num The page number of the internal node.
 * @param key The key to find in the internal node.
 * @return Cursor pointing to the location of the key.
 */

Cursor* internal_node_find(Table* table, uint32_t page_num, uint32_t key) {
  void* node = get_page(table->pager, page_num);  // Retrieve the specified page
  // Find the child that should contain the key
  uint32_t child_index = internal_node_find_child(node, key);
  uint32_t child_num = *internal_node_child(node, child_index);
  void* child = get_page(table->pager, child_num);
  // Recursively find the key in the child node
  switch (get_node_type(child)) {
    case NODE_LEAF:
      return leaf_node_find(table, child_num, key);
    case NODE_INTERNAL:
      return internal_node_find(table, child_num, key);
  }
}

/**
 * Finds the position of a given key in the table. If the key is not present, returns the position where it should be inserted.
 * @param table Pointer to the Table structure.
 * @param key The key to find.
 * @return Cursor pointing to the location of the key or where it should be inserted.
 */

Cursor* table_find(Table* table, uint32_t key) {
  uint32_t root_page_num = table->root_page_num;
  void* root_node = get_page(table->pager, root_page_num);
  // Determine the type of the root node and find the key accordingly
  if (get_node_type(root_node) == NODE_LEAF) {
    return leaf_node_find(table, root_page_num, key);
  } else {
    return internal_node_find(table, root_page_num, key);
  }
}

/**
 * Returns a cursor to the start of the table.
 * @param table Pointer to the Table structure.
 * @return Cursor positioned at the start of the table.
 */

Cursor* table_start(Table* table) {
  Cursor* cursor = table_find(table, 0);  // Find the first position in the table

  void* node = get_page(table->pager, cursor->page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);
  cursor->end_of_table = (num_cells == 0);  // Set end_of_table if the table is empty

  return cursor;
}

/**
 * Retrieves the value at the cursor's current position.
 * @param cursor Pointer to the Cursor structure.
 * @return Pointer to the value at the cursor's current position.
 */

void* cursor_value(Cursor* cursor) {
  uint32_t page_num = cursor->page_num;
  void* page = get_page(cursor->table->pager, page_num);
  return leaf_node_value(page, cursor->cell_num); // Return the value at the cursor's position
}

/**
 * Advances the cursor to the next position in the table.
 * @param cursor Pointer to the Cursor structure to advance.
 */

void cursor_advance(Cursor* cursor) {
  uint32_t page_num = cursor->page_num;
  void* node = get_page(cursor->table->pager, page_num);

  cursor->cell_num += 1;  // Move to the next cell
  // Check if we need to move to the next leaf node
  if (cursor->cell_num >= (*leaf_node_num_cells(node))) {
    /* Advance to next leaf node */
    uint32_t next_page_num = *leaf_node_next_leaf(node);
    if (next_page_num == 0) {
      /* This was rightmost leaf */
      cursor->end_of_table = true;
    } else {
      // Move to the next leaf node
      cursor->page_num = next_page_num;
      cursor->cell_num = 0;
    }
  }
}

/**
 * Opens the database file and initializes a Pager structure.
 * @param filename Name of the database file to open.
 * @return Pointer to the initialized Pager structure.
 */

Pager* pager_open(const char* filename) {
// Open the database file with read/write permissions; create if it doesn't exist
  int fd = open(filename,
                O_RDWR |      // Read/Write mode
                    O_CREAT,  // Create file if it does not exist
                S_IWUSR |     // User write permission
                    S_IRUSR   // User read permission
                );

  if (fd == -1) {
    printf("Unable to open file\n");
    exit(EXIT_FAILURE);
  }
  // Determine the length of the file
  off_t file_length = lseek(fd, 0, SEEK_END);
  // Allocate and initialize the Pager structure
  Pager* pager = malloc(sizeof(Pager));
  pager->file_descriptor = fd;
  pager->file_length = file_length;
  pager->num_pages = (file_length / PAGE_SIZE);
  // Check for file corruption: file length should be a multiple of PAGE_SIZE
  if (file_length % PAGE_SIZE != 0) {
    printf("Db file is not a whole number of pages. Corrupt file.\n");
    exit(EXIT_FAILURE);
  }
  // Initialize all page pointers to NULL
  for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
    pager->pages[i] = NULL;
  }

  return pager;
}

/**
 * Opens a database file and initializes a Table structure.
 * @param filename Name of the database file to open.
 * @return Pointer to the initialized Table structure.
 */

Table* db_open(const char* filename) {
  // Open the pager for the database file
  Pager* pager = pager_open(filename);
  // Allocate and initialize the Table structure
  Table* table = malloc(sizeof(Table));
  table->pager = pager;
  table->root_page_num = 0;

  if (pager->num_pages == 0) {
    // New database file. Initialize page 0 as leaf node.
    void* root_node = get_page(pager, 0);
    initialize_leaf_node(root_node);
    set_node_root(root_node, true);
  }

  return table;
}

#define MAX_QUERY_LEN 1024  // Define the maximum length for the query string

/**
 * Executes the Python script to convert natural language input to a database query.
 * 
 * @param user_input The natural language input from the user.
 * @return A string containing the translated database query.
 */
char* get_db_query(const char* user_input) {
    char command[MAX_QUERY_LEN];
    sprintf(command, "python3 model_old/lora.py \"%s\"", user_input);
    printf("Executing command: %s\n", command);  // Debug print

    FILE* fp = popen(command, "r");
    if (fp == NULL) {
        printf("Failed to run command\n");
        exit(1);
    }

    static char translated_query[MAX_QUERY_LEN];
    if (fgets(translated_query, sizeof(translated_query), fp) == NULL) {
        printf("Failed to read output\n");  // Debug print
    }
    printf("Script output before trimming: '%s'\n", translated_query);  // Debug print

    // Trim newline character
    char* newline = strchr(translated_query, '\n');
    if (newline) {
        *newline = '\0';  // Replace newline with null terminator
    }

    printf("Script output after trimming: '%s'\n", translated_query);  // Debug print

    pclose(fp);
    return translated_query;
}

/**
 * Creates a new input buffer for user input.
 * @return Pointer to the initialized InputBuffer structure.
 */

InputBuffer* new_input_buffer() {
// Allocate and initialize the InputBuffer structure  
  InputBuffer* input_buffer = malloc(sizeof(InputBuffer));
  input_buffer->buffer = NULL;
  input_buffer->buffer_length = 0;
  input_buffer->input_length = 0;

  return input_buffer;
}

/**
 * Prints the database prompt.
 */

void print_prompt() { printf("db > "); }

/**
 * Reads user input into an InputBuffer.
 * @param input_buffer Pointer to the InputBuffer structure.
 */

void read_input(InputBuffer* input_buffer) {
  ssize_t bytes_read =
      getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);
  // Error handling for getline failure
  if (bytes_read <= 0) {
    printf("Error reading input\n");
    exit(EXIT_FAILURE);
  }

  // Ignore trailing newline
  input_buffer->input_length = bytes_read - 1;
  input_buffer->buffer[bytes_read - 1] = 0;
}

/**
 * Closes and frees an InputBuffer.
 * @param input_buffer Pointer to the InputBuffer structure to close.
 */

void close_input_buffer(InputBuffer* input_buffer) {
  // Free the buffer and the InputBuffer structure itself
  free(input_buffer->buffer);
  free(input_buffer);
}

/**
 * Flushes a page to disk.
 * @param pager Pointer to the Pager structure.
 * @param page_num The page number to flush.
 */

void pager_flush(Pager* pager, uint32_t page_num) {
  // Error handling for null page
  if (pager->pages[page_num] == NULL) {
    printf("Tried to flush null page\n");
    exit(EXIT_FAILURE);
  }
  // Seek to the correct location in the file
  off_t offset = lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);

  if (offset == -1) {
    printf("Error seeking: %d\n", errno);
    exit(EXIT_FAILURE);
  }
  // Write the page to disk
  ssize_t bytes_written =
      write(pager->file_descriptor, pager->pages[page_num], PAGE_SIZE);

  if (bytes_written == -1) {
    printf("Error writing: %d\n", errno);
    exit(EXIT_FAILURE);
  }
}

/**
 * Closes the database, flushing all pages to disk.
 * @param table Pointer to the Table structure.
 */

void db_close(Table* table) {
  Pager* pager = table->pager;
  // Flush all pages to disk
  for (uint32_t i = 0; i < pager->num_pages; i++) {
    if (pager->pages[i] == NULL) {
      continue;
    }
    pager_flush(pager, i);
    free(pager->pages[i]);
    pager->pages[i] = NULL;
  }
  // Close the file descriptor
  int result = close(pager->file_descriptor);
  if (result == -1) {
    printf("Error closing db file.\n");
    exit(EXIT_FAILURE);
  }
  for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
    void* page = pager->pages[i];
    if (page) {
      free(page);
      pager->pages[i] = NULL;
    }
  }
  // Free the Pager and Table structures
  free(pager);
  free(table);
}

/**
 * Executes a meta-command.
 * @param input_buffer Pointer to the InputBuffer containing the command.
 * @param table Pointer to the Table structure.
 * @return Result of the meta-command execution.
 */

MetaCommandResult do_meta_command(InputBuffer* input_buffer, Table* table) {
  // Handle the ".exit" command
  if (strcmp(input_buffer->buffer, ".exit") == 0) {
    close_input_buffer(input_buffer);
    db_close(table);
    exit(EXIT_SUCCESS);
    // Handle the ".btree" command to print the B-tree
  } else if (strcmp(input_buffer->buffer, ".btree") == 0) {
    printf("Tree:\n");
    print_tree(table->pager, 0, 0);
    return META_COMMAND_SUCCESS;
    // Handle the ".constants" command to print constants
  } else if (strcmp(input_buffer->buffer, ".constants") == 0) {
    printf("Constants:\n");
    print_constants();
    return META_COMMAND_SUCCESS;
  } else {
    return META_COMMAND_UNRECOGNIZED_COMMAND;
  }
}

/**
 * Prepares an INSERT statement.
 * @param input_buffer Pointer to the InputBuffer containing the command.
 * @param statement Pointer to the Statement structure to be prepared.
 * @return Result of the preparation process.
 */

PrepareResult prepare_insert(InputBuffer* input_buffer, Statement* statement) {
  statement->type = STATEMENT_INSERT;

  // Tokenize the input to extract individual components
  char* keyword = strtok(input_buffer->buffer, " ");
  char* username = strtok(NULL, " ");  // First input is now name
  char* id_string = strtok(NULL, " "); // Second input is id
  char* email = strtok(NULL, " ");     // Third input is email

  // Syntax validation
  if (id_string == NULL || username == NULL || email == NULL) {
    return PREPARE_SYNTAX_ERROR;
  }

  int id = atoi(id_string); // Convert id to integer
  if (id < 0) {
    return PREPARE_NEGATIVE_ID;
  }
  if (strlen(username) > COLUMN_USERNAME_SIZE) {
    return PREPARE_STRING_TOO_LONG;
  }
  if (strlen(email) > COLUMN_EMAIL_SIZE) {
    return PREPARE_STRING_TOO_LONG;
  }
  // Set the values in the statement
  statement->row_to_insert.id = id;
  strcpy(statement->row_to_insert.username, username);
  strcpy(statement->row_to_insert.email, email);

  return PREPARE_SUCCESS;
}


/**
 * Prepares a statement based on input.
 * @param input_buffer Pointer to the InputBuffer containing the command.
 * @param statement Pointer to the Statement structure to be prepared.
 * @return Result of the preparation process.
 */

PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement) {
    // Check if input is a natural language command
    if (strncmp(input_buffer->buffer, "Ada ", 4) == 0) {
        // Call Python script to get SQL query
        char* sql_query = get_db_query(input_buffer->buffer + 4);
        strncpy(input_buffer->buffer, sql_query, input_buffer->buffer_length - 1);
        input_buffer->buffer[input_buffer->buffer_length - 1] = '\0'; // Ensure null termination
    }

    // Existing logic to prepare INSERT and SELECT statements
    if (strncmp(input_buffer->buffer, "insert", 6) == 0) {
        return prepare_insert(input_buffer, statement);
    }
    if (strcmp(input_buffer->buffer, "select") == 0) {
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }

    // Handle unrecognized statements
    return PREPARE_UNRECOGNIZED_STATEMENT;
}


/**
 * Gets the number of the next unused page in the pager.
 * @param pager Pointer to the Pager structure.
 * @return The page number of the next unused page.
 */

uint32_t get_unused_page_num(Pager* pager) { 
  // Return the current number of pages as the next unused page number
  return pager->num_pages; 
}

/**
 * Handles splitting the root node and creating a new root.
 * @param table Pointer to the Table structure.
 * @param right_child_page_num Page number of the new right child.
 */

void create_new_root(Table* table, uint32_t right_child_page_num) {
  // Retrieve the current root and right child
  void* root = get_page(table->pager, table->root_page_num);
  void* right_child = get_page(table->pager, right_child_page_num);
  // Allocate a new page to move the old root data
  uint32_t left_child_page_num = get_unused_page_num(table->pager);
  void* left_child = get_page(table->pager, left_child_page_num);
  // Initialize the new left and right children as internal nodes if necessary
  if (get_node_type(root) == NODE_INTERNAL) {
    initialize_internal_node(right_child);
    initialize_internal_node(left_child);
  }

  /* Left child has data copied from old root */
  memcpy(left_child, root, PAGE_SIZE);
  set_node_root(left_child, false);
  // Update the parent pointers of all children of the left child
  if (get_node_type(left_child) == NODE_INTERNAL) {
    void* child;
    for (int i = 0; i < *internal_node_num_keys(left_child); i++) {
      child = get_page(table->pager, *internal_node_child(left_child,i));
      *node_parent(child) = left_child_page_num;
    }
    child = get_page(table->pager, *internal_node_right_child(left_child));
    *node_parent(child) = left_child_page_num;
  }

  /* Root node is a new internal node with one key and two children */
  initialize_internal_node(root);
  set_node_root(root, true);
  *internal_node_num_keys(root) = 1;
  *internal_node_child(root, 0) = left_child_page_num;
  uint32_t left_child_max_key = get_node_max_key(table->pager, left_child);
  *internal_node_key(root, 0) = left_child_max_key;
  *internal_node_right_child(root) = right_child_page_num;
  *node_parent(left_child) = table->root_page_num;
  *node_parent(right_child) = table->root_page_num;
}



void internal_node_split_and_insert(Table* table, uint32_t parent_page_num,
                          uint32_t child_page_num);

/**
 * Adds a new child/key pair to a parent internal node.
 * @param table Pointer to the Table structure.
 * @param parent_page_num Page number of the parent internal node.
 * @param child_page_num Page number of the child node to be added.
 */

void internal_node_insert(Table* table, uint32_t parent_page_num,
                          uint32_t child_page_num) {
  /*
  Add a new child/key pair to parent that corresponds to child
  */

  void* parent = get_page(table->pager, parent_page_num);
  void* child = get_page(table->pager, child_page_num);
  uint32_t child_max_key = get_node_max_key(table->pager, child);
  uint32_t index = internal_node_find_child(parent, child_max_key);

  uint32_t original_num_keys = *internal_node_num_keys(parent);

  if (original_num_keys >= INTERNAL_NODE_MAX_KEYS) {
    internal_node_split_and_insert(table, parent_page_num, child_page_num);
    return;
  }

  uint32_t right_child_page_num = *internal_node_right_child(parent);
  /*
  An internal node with a right child of INVALID_PAGE_NUM is empty
  */
  if (right_child_page_num == INVALID_PAGE_NUM) {
    *internal_node_right_child(parent) = child_page_num;
    return;
  }

  void* right_child = get_page(table->pager, right_child_page_num);
  /*
  If we are already at the max number of cells for a node, we cannot increment
  before splitting. Incrementing without inserting a new key/child pair
  and immediately calling internal_node_split_and_insert has the effect
  of creating a new key at (max_cells + 1) with an uninitialized value
  */
  *internal_node_num_keys(parent) = original_num_keys + 1;

  if (child_max_key > get_node_max_key(table->pager, right_child)) {
    /* Replace right child */
    *internal_node_child(parent, original_num_keys) = right_child_page_num;
    *internal_node_key(parent, original_num_keys) =
        get_node_max_key(table->pager, right_child);
    *internal_node_right_child(parent) = child_page_num;
  } else {
    /* Make room for the new cell */
    for (uint32_t i = original_num_keys; i > index; i--) {
      void* destination = internal_node_cell(parent, i);
      void* source = internal_node_cell(parent, i - 1);
      memcpy(destination, source, INTERNAL_NODE_CELL_SIZE);
    }
    *internal_node_child(parent, index) = child_page_num;
    *internal_node_key(parent, index) = child_max_key;
  }
}

/**
 * Updates a key in an internal node.
 * @param node Pointer to the internal node.
 * @param old_key The key to be replaced.
 * @param new_key The new key to replace with.
 */

void update_internal_node_key(void* node, uint32_t old_key, uint32_t new_key) {
  uint32_t old_child_index = internal_node_find_child(node, old_key);
  *internal_node_key(node, old_child_index) = new_key;
}

/**
 * Splits an internal node and inserts a new child.
 * @param table Pointer to the Table structure.
 * @param parent_page_num Page number of the parent internal node.
 * @param child_page_num Page number of the child node to be inserted.
 */

void internal_node_split_and_insert(Table* table, uint32_t parent_page_num,
                          uint32_t child_page_num) {
  uint32_t old_page_num = parent_page_num;
  void* old_node = get_page(table->pager,parent_page_num);
  uint32_t old_max = get_node_max_key(table->pager, old_node);

  void* child = get_page(table->pager, child_page_num); 
  uint32_t child_max = get_node_max_key(table->pager, child);

  uint32_t new_page_num = get_unused_page_num(table->pager);

  /*
  Declaring a flag before updating pointers which
  records whether this operation involves splitting the root -
  if it does, we will insert our newly created node during
  the step where the table's new root is created. If it does
  not, we have to insert the newly created node into its parent
  after the old node's keys have been transferred over. We are not
  able to do this if the newly created node's parent is not a newly
  initialized root node, because in that case its parent may have existing
  keys aside from our old node which we are splitting. If that is true, we
  need to find a place for our newly created node in its parent, and we
  cannot insert it at the correct index if it does not yet have any keys
  */
  uint32_t splitting_root = is_node_root(old_node);

  void* parent;
  void* new_node;
  if (splitting_root) {
    create_new_root(table, new_page_num);
    parent = get_page(table->pager,table->root_page_num);
    /*
    If we are splitting the root, we need to update old_node to point
    to the new root's left child, new_page_num will already point to
    the new root's right child
    */
    old_page_num = *internal_node_child(parent,0);
    old_node = get_page(table->pager, old_page_num);
  } else {
    parent = get_page(table->pager,*node_parent(old_node));
    new_node = get_page(table->pager, new_page_num);
    initialize_internal_node(new_node);
  }
  
  uint32_t* old_num_keys = internal_node_num_keys(old_node);

  uint32_t cur_page_num = *internal_node_right_child(old_node);
  void* cur = get_page(table->pager, cur_page_num);

  /*
  First put right child into new node and set right child of old node to invalid page number
  */
  internal_node_insert(table, new_page_num, cur_page_num);
  *node_parent(cur) = new_page_num;
  *internal_node_right_child(old_node) = INVALID_PAGE_NUM;
  /*
  For each key until you get to the middle key, move the key and the child to the new node
  */
  for (int i = INTERNAL_NODE_MAX_KEYS - 1; i > INTERNAL_NODE_MAX_KEYS / 2; i--) {
    cur_page_num = *internal_node_child(old_node, i);
    cur = get_page(table->pager, cur_page_num);

    internal_node_insert(table, new_page_num, cur_page_num);
    *node_parent(cur) = new_page_num;

    (*old_num_keys)--;
  }

  /*
  Set child before middle key, which is now the highest key, to be node's right child,
  and decrement number of keys
  */
  *internal_node_right_child(old_node) = *internal_node_child(old_node,*old_num_keys - 1);
  (*old_num_keys)--;

  /*
  Determine which of the two nodes after the split should contain the child to be inserted,
  and insert the child
  */
  uint32_t max_after_split = get_node_max_key(table->pager, old_node);

  uint32_t destination_page_num = child_max < max_after_split ? old_page_num : new_page_num;

  internal_node_insert(table, destination_page_num, child_page_num);
  *node_parent(child) = destination_page_num;

  update_internal_node_key(parent, old_max, get_node_max_key(table->pager, old_node));

  if (!splitting_root) {
    internal_node_insert(table,*node_parent(old_node),new_page_num);
    *node_parent(new_node) = *node_parent(old_node);
  }
}

/**
 * Splits a leaf node and inserts a new key-value pair into the appropriate leaf node.
 * This function is called when the current leaf node is full and needs to split into two nodes.
 * 
 * @param cursor Pointer to the Cursor structure representing the position in the tree.
 * @param key The key to be inserted.
 * @param value Pointer to the Row structure representing the value to be inserted.
 */

void leaf_node_split_and_insert(Cursor* cursor, uint32_t key, Row* value) {
  /*
  Create a new node and move half the cells over.
  Insert the new value in one of the two nodes.
  Update parent or create a new parent.
  */

  void* old_node = get_page(cursor->table->pager, cursor->page_num);
  uint32_t old_max = get_node_max_key(cursor->table->pager, old_node);
  uint32_t new_page_num = get_unused_page_num(cursor->table->pager);
  void* new_node = get_page(cursor->table->pager, new_page_num);
  initialize_leaf_node(new_node);
  *node_parent(new_node) = *node_parent(old_node);
  *leaf_node_next_leaf(new_node) = *leaf_node_next_leaf(old_node);
  *leaf_node_next_leaf(old_node) = new_page_num;

  /*
  All existing keys plus new key should should be divided
  evenly between old (left) and new (right) nodes.
  Starting from the right, move each key to correct position.
  */
  for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--) {
    void* destination_node;
    if (i >= LEAF_NODE_LEFT_SPLIT_COUNT) {
      destination_node = new_node;
    } else {
      destination_node = old_node;
    }
    uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;
    void* destination = leaf_node_cell(destination_node, index_within_node);

    if (i == cursor->cell_num) {
      serialize_row(value,
                    leaf_node_value(destination_node, index_within_node));
      *leaf_node_key(destination_node, index_within_node) = key;
    } else if (i > cursor->cell_num) {
      memcpy(destination, leaf_node_cell(old_node, i - 1), LEAF_NODE_CELL_SIZE);
    } else {
      memcpy(destination, leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
    }
  }

  /* Update cell count on both leaf nodes */
  *(leaf_node_num_cells(old_node)) = LEAF_NODE_LEFT_SPLIT_COUNT;
  *(leaf_node_num_cells(new_node)) = LEAF_NODE_RIGHT_SPLIT_COUNT;

  if (is_node_root(old_node)) {
    return create_new_root(cursor->table, new_page_num);
  } else {
    uint32_t parent_page_num = *node_parent(old_node);
    uint32_t new_max = get_node_max_key(cursor->table->pager, old_node);
    void* parent = get_page(cursor->table->pager, parent_page_num);

    update_internal_node_key(parent, old_max, new_max);
    internal_node_insert(cursor->table, parent_page_num, new_page_num);
    return;
  }
}

/**
 * Inserts a new key-value pair into a leaf node. If the leaf node is full, it splits the node
 * and then inserts the new key-value pair.
 * 
 * @param cursor Pointer to the Cursor structure representing the position in the tree.
 * @param key The key to be inserted.
 * @param value Pointer to the Row structure representing the value to be inserted.
 */

void leaf_node_insert(Cursor* cursor, uint32_t key, Row* value) {
  void* node = get_page(cursor->table->pager, cursor->page_num);

  uint32_t num_cells = *leaf_node_num_cells(node);
  if (num_cells >= LEAF_NODE_MAX_CELLS) {
    // Node full
    leaf_node_split_and_insert(cursor, key, value);
    return;
  }

  if (cursor->cell_num < num_cells) {
    // Make room for new cell
    for (uint32_t i = num_cells; i > cursor->cell_num; i--) {
      memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i - 1),
             LEAF_NODE_CELL_SIZE);
    }
  }

  *(leaf_node_num_cells(node)) += 1;
  *(leaf_node_key(node, cursor->cell_num)) = key;
  serialize_row(value, leaf_node_value(node, cursor->cell_num));
}

/**
 * Executes an insert operation in the database. This function inserts a new row into a table.
 * It handles the insertion of the row and checks for duplicate keys.
 * 
 * @param statement Pointer to the Statement structure containing the row to insert.
 * @param table Pointer to the Table structure where the row will be inserted.
 * @return ExecuteResult indicating the result of the execution (success or error code).
 */

ExecuteResult execute_insert(Statement* statement, Table* table) {
  // Extract row to insert and its key.
  Row* row_to_insert = &(statement->row_to_insert);
  uint32_t key_to_insert = row_to_insert->id;
  // Find the position to insert the new row.
  Cursor* cursor = table_find(table, key_to_insert);
  // Access the node where the row will be inserted.
  void* node = get_page(table->pager, cursor->page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);
  // Check for duplicate keys.
  if (cursor->cell_num < num_cells) {
    uint32_t key_at_index = *leaf_node_key(node, cursor->cell_num);
    if (key_at_index == key_to_insert) {
      return EXECUTE_DUPLICATE_KEY;
    }
  }
  // Perform the insertion.
  leaf_node_insert(cursor, row_to_insert->id, row_to_insert);
  // Clean up.
  free(cursor);

  return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Statement* statement, Table* table) {
  Cursor* cursor = table_start(table);

  if (cursor->end_of_table) {
    printf("DB is empty.\n");
    free(cursor);
    return EXECUTE_SUCCESS;
  }

  Row row;
  while (!(cursor->end_of_table)) {
    deserialize_row(cursor_value(cursor), &row);
    print_row(&row);
    cursor_advance(cursor);
  }

  free(cursor);

  return EXECUTE_SUCCESS;
}


/**
 * Executes a statement depending on its type (e.g., insert, select).
 * 
 * @param statement Pointer to the Statement structure representing the operation to execute.
 * @param table Pointer to the Table structure representing the database table.
 * @return ExecuteResult indicating the result of the execution.
 */

ExecuteResult execute_statement(Statement* statement, Table* table) {
  switch (statement->type) {
    case (STATEMENT_INSERT):
      return execute_insert(statement, table);
    case (STATEMENT_SELECT):
      return execute_select(statement, table);
  }
}

/**
 * The main function of the database application. It handles command-line arguments, 
 * initializes the database, processes input, and executes commands.
 */
int main(int argc, char* argv[]) {
  //Printing Header
  printf("Welcome to the database\n \n \n");
  if (argc < 2) {
      printf("Must supply a database filename.\n");
      exit(EXIT_FAILURE);
  }

  char* filename = argv[1];
  Table* table = db_open(filename);

  InputBuffer* input_buffer = new_input_buffer();
  while (true) {
      print_prompt();
      read_input(input_buffer);

      if (input_buffer->buffer[0] == '.') {
          switch (do_meta_command(input_buffer, table)) {
              case META_COMMAND_SUCCESS:
                  continue;
              case META_COMMAND_UNRECOGNIZED_COMMAND:
                  printf("Unrecognized command '%s'\n", input_buffer->buffer);
                  continue;
          }
      }

      Statement statement;
      switch (prepare_statement(input_buffer, &statement)) {
          case PREPARE_SUCCESS:
              break;
          case PREPARE_NEGATIVE_ID:
              printf("ID must be positive.\n");
              continue;
          case PREPARE_STRING_TOO_LONG:
              printf("String is too long.\n");
              continue;
          case PREPARE_SYNTAX_ERROR:
              printf("Syntax error. Could not parse statement.\n");
              continue;
          case PREPARE_UNRECOGNIZED_STATEMENT:
              printf("Unrecognized keyword at start of '%s'.\n", input_buffer->buffer);
              continue;
      }

      switch (execute_statement(&statement, table)) {
          case EXECUTE_SUCCESS:
              // printf("Executed.\n");
              break;
          case EXECUTE_DUPLICATE_KEY:
              printf("Error: Duplicate key.\n");
              break;
      }
  }
}