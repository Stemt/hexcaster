#define NOB_IMPLEMENTATION
#include "nob.h"

#define NHL_APP_INTERFACE
#include "nhl.h"

#include <raylib.h>

typedef struct{
  char pad[1024];
} App;

typedef struct{
  int a;
  float b;
  char str[16];
} TestData;

typedef struct{
  struct {
    float size;
    float spacing;
    Color color;
  } text;
  Color background;
  struct{
    struct{
      Color background;
    } up;
    struct{
      Color background;
    } hover;
    struct{
      Color background;
    } down;
    struct{
      Color color;
      float size;
    } border;
  } button;
} Style;

static Style style = {
  .text = {
    .size = 20.0f,
    .spacing = 2.0f,
    .color = WHITE,
  },
  .button = {
    .down = {
      .background = RAYWHITE,
    },
    .up = {
      .background = DARKGRAY,
    },
    .hover = {
      .background = GRAY,
    },
    .border = {
      .color = GRAY,
      .size = 2,
    },
  },
  .background = DARKGRAY,
};

Rectangle rect_offset(Rectangle rect, float offset){
  return (Rectangle){
    .x = rect.x - offset,
    .y = rect.y - offset,
    .width = rect.width + 2*offset,
    .height = rect.height + 2*offset,
  };
}

typedef struct{
  int width;
  int height;
} TableCellOpts;
#define rect_table_cell(rect, cols, rows, col, row, ...)\
  rect_table_cell_opt((rect), (cols), (rows), (col), (row), (TableCellOpts){__VA_ARGS__})

Rectangle rect_table_cell_opt(Rectangle rect, int cols, int rows, int col, int row, TableCellOpts opts){
  if(opts.width == 0) opts.width = 1;
  if(opts.height == 0) opts.height = 1;

  Rectangle cell_rect = {0};
  cell_rect.width = (rect.width/cols);
  cell_rect.height = (rect.height/rows);
  cell_rect.x = rect.x + cell_rect.width*col;
  cell_rect.y = rect.y + cell_rect.height*row;
  cell_rect.width *= opts.width;
  cell_rect.height *= opts.height;
  return cell_rect;
}

typedef struct{
  union{
    Rectangle top;
    Rectangle left;
  };
  union{
    Rectangle bottom;
    Rectangle right;
  };
} Split;

typedef struct{
  float vertical;
  float horizontal;
  float vertical_px;
  float horizontal_px;
} RectSplitOpts;
#define rect_split(rect, ...) rect_split_opt((rect), (RectSplitOpts){__VA_ARGS__})
Split rect_split_opt(Rectangle rect, RectSplitOpts opts){
  if(opts.horizontal > 0){
    Split split = {
      .left = rect,
      .right = rect
    };
    split.left.width *= opts.horizontal;
    split.right.x += split.left.width;
    split.right.width -= split.left.width;
    return split;
  }else if(opts.vertical > 0){
    Split split = {
      .top = rect,
      .bottom = rect
    };
    split.top.height *= opts.vertical;
    split.bottom.y += split.top.height;
    split.bottom.height -= split.top.height;
    return split;
  }else if(opts.horizontal_px > 0){
    Split split = {
      .left = rect,
      .right = rect
    };
    split.left.width = opts.horizontal_px;
    split.right.width = rect.width-opts.horizontal_px;
    split.right.x += opts.horizontal_px;
    return split;
  }else if(opts.vertical_px > 0){
    Split split = {
      .top = rect,
      .bottom = rect
    };
    split.top.height = opts.vertical_px;
    split.bottom.height = rect.height-opts.vertical_px;
    split.bottom.y += opts.vertical_px;
    return split;
  }else{
    nob_log(NOB_ERROR, "split_rect: invalid arguments, one should be true .vertical > 0 || .horizontal > 0");
    abort();
  }
}

typedef enum{
  Align_CENTER = 0,
  Align_LEFT = (1<<0),
  Align_RIGHT = (1<<1),
  Align_TOP = (1<<2),
  Align_BOTTOM = (1<<3),
} Align;

typedef struct{
  Align align;
} LabelOpts;
#define label(rect, text, ...) label_opt((rect), (text), (LabelOpts){__VA_ARGS__})
void label_opt(Rectangle rect, const char* text, LabelOpts opts){
  Vector2 size = MeasureTextEx(GetFontDefault(), text, style.text.size, style.text.spacing);

  Rectangle text_rect = {
    rect.x + (rect.width/2-size.x/2),
    rect.y + (rect.height/2-size.y/2),
    size.x,size.y
  };
  
  if(opts.align & Align_LEFT){
    text_rect.x = rect.x;
  };
  if(opts.align & Align_RIGHT){
    text_rect.x = rect.x+rect.width-size.x;
  };
  if(opts.align & Align_TOP){
    text_rect.y = rect.y;
  };
  if(opts.align & Align_BOTTOM){
    text_rect.y = rect.y+rect.height-size.y;
  };

  DrawText(text, text_rect.x, text_rect.y, style.text.size, style.text.color);
}

static bool mouse_cursor_set = false;

bool hover(Rectangle rect){
  return CheckCollisionPointRec(GetMousePosition(), rect);
}

typedef struct{
  Align align;
} ButtonOpts;
#define button(rect, text, ...) button_opt((rect), (text), (ButtonOpts){__VA_ARGS__})
bool button_opt(Rectangle rect, const char* text, ButtonOpts opts){

  if(hover(rect)){
    SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
    mouse_cursor_set = true;
  }
  if(hover(rect) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)){
    DrawRectangleRec(rect, style.button.down.background);
  }else if(hover(rect)){
    DrawRectangleRec(rect, style.button.hover.background);
  }else{
    DrawRectangleRec(rect, style.button.up.background);
  }
  DrawRectangleLinesEx(rect, style.button.border.size, style.button.border.color);
  label(rect,text, .align = opts.align);
  return hover(rect) && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
}

typedef enum{
  Type_STRUCT = 0,
  Type_INT,
  Type_FLOAT,
  Type_CHAR_ARRAY,
} TypeKind;

typedef struct Type Type;
struct Type{
  TypeKind kind;
  Type* parent;
  size_t id;
  union{
    struct{
      Type* items;
      size_t count;
      size_t capacity;
    } Struct;
    struct{
      size_t count;
    } Array;
  } as;
};

typedef struct{
  Type* items;
  size_t count;
  size_t capacity;
} TypeArray;

static TypeArray type_arena = {0};
static TypeArray current_struct = {0};

void Struct_remove(Type* self, size_t i){
  if(self->kind != Type_STRUCT) return;
  if(i < self->as.Struct.count-1){
    memcpy(
        self->as.Struct.items+i, 
        self->as.Struct.items+i+1, 
        self->as.Struct.count-i-1);
  }
  self->as.Struct.count--;
}

void Struct_add(Type* self, Type child){
  child.parent = self;
  child.id = self->as.Struct.count;
  nob_da_append(&self->as.Struct, child);
}

size_t Type_sizeof(Type* self){
  switch (self->kind) {
    case Type_INT: return 4;
    case Type_FLOAT: return 4;
    case Type_CHAR_ARRAY: return self->as.Array.count;
    case Type_STRUCT:{
      size_t size = 0;
      for(size_t i = 0; i < self->as.Struct.count; ++i){
        size += Type_sizeof(&self->as.Struct.items[i]);
      }
      return size;
    };
  }
  return 0;
}

static Type* dragging_type = NULL;
static Rectangle dragging_rect = {0};

void Type_render(Rectangle rect, Type* self, void* buffer){
  int padding = 2;
  int row_height = style.text.size+padding*2;
  int rows = rect.height/row_height;
  int cols = 1;

  size_t type_size = Type_sizeof(self);
  const char* type_text = "<undefined>";
  Rectangle sub_rect = rect_table_cell(rect, cols, rows, 0, 0, .height = type_size);

  switch(self->kind){
    case Type_STRUCT:{
      if(hover(sub_rect) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)){
        dragging_type = self;
        dragging_rect = sub_rect;
      }else if(IsMouseButtonUp(MOUSE_LEFT_BUTTON)){
        dragging_type = NULL;
      }

      //DrawRectangleRec(sub_rect, ColorAlpha(GREEN, 0.2));
      //DrawRectangleLinesEx(sub_rect, 2, GREEN);
      size_t offset = 0;
      for(size_t i = 0; i < self->as.Struct.count; ++i){
        type_size = Type_sizeof(&self->as.Struct.items[i]);
        sub_rect = rect_table_cell(rect, cols, rows, 0, offset, .height=type_size);
        void* offset_buffer = buffer == NULL ? NULL : buffer + offset;
        Type_render(sub_rect, &self->as.Struct.items[i], offset_buffer);
        offset += type_size;
      }
      if(buffer != NULL || self == dragging_type) return;

      sub_rect = rect_table_cell(rect, cols, rows, 0, offset, .height=1);
      static bool add_type = false;
      if(!add_type && button(sub_rect, "+")){
        add_type = true;
      }else if(add_type){
        offset++;
        sub_rect = rect_table_cell(rect, cols, rows, 0, offset, .height=1);
        if(button(sub_rect, "Int")){
          Struct_add(self, ((Type){
            .kind = Type_INT,
          }));
          add_type = false;
        }
        offset++;
        sub_rect = rect_table_cell(rect, cols, rows, 0, offset, .height=1);
        if(button(sub_rect, "Float")){
          Struct_add(self, ((Type){
            .kind = Type_FLOAT,
          }));
          add_type = false;
        }
        offset++;
        sub_rect = rect_table_cell(rect, cols, rows, 0, offset, .height=1);
        if(button(sub_rect, "Char[]")){
          Struct_add(self, ((Type){
            .kind = Type_CHAR_ARRAY,
          }));
          add_type = false;
        }
        offset++;
        sub_rect = rect_table_cell(rect, cols, rows, 0, offset, .height=1);
        if(button(sub_rect, "Cancel")){
          add_type = false;
        }
      }
    }break;
    case Type_INT:{
      DrawRectangleRec(sub_rect, ColorAlpha(BLUE, 0.2));
      DrawRectangleLinesEx(sub_rect, 2, BLUE);
      if(buffer == NULL){
        label(sub_rect, "Int");
        if(hover(sub_rect) && button(rect_table_cell(sub_rect, 3, type_size, 0, 0), "x")){
          Struct_remove(self->parent, self->id);
        }
      }else{
        label(sub_rect, nob_temp_sprintf("Int: %d", *(int*)buffer));
      }
    }break;
    case Type_FLOAT:{
      DrawRectangleRec(sub_rect, ColorAlpha(YELLOW, 0.2));
      DrawRectangleLinesEx(sub_rect, 2, YELLOW);
      if(buffer == NULL){
        label(sub_rect, "Float");
        if(hover(sub_rect) && button(rect_table_cell(sub_rect, 3, type_size, 0, 0), "x")){
          Struct_remove(self->parent, self->id);
        }
      }else{
        label(sub_rect, nob_temp_sprintf("Float: %f", *(float*)buffer));
      }
    }break;
    case Type_CHAR_ARRAY:{
      DrawRectangleRec(sub_rect, ColorAlpha(PURPLE, 0.2));
      DrawRectangleLinesEx(sub_rect, 2, PURPLE);
      if(buffer == NULL){
        if(self->as.Array.count <= 0) self->as.Array.count = 1;
        label(sub_rect, nob_temp_sprintf("Char[%lu]", self->as.Array.count));
        if(hover(sub_rect) && button(rect_table_cell(sub_rect, 3, type_size, 0, 0), "x")){
          Struct_remove(self->parent, self->id);
        }
        if(button(rect_table_cell(sub_rect, 8, type_size, 7, 0), "+")) 
          self->as.Array.count++;
        if(button(rect_table_cell(sub_rect, 8, type_size, 6, 0), "-"))
          self->as.Array.count--;
      }else{
        label(sub_rect, nob_temp_sprintf("Char[%lu]: %.*s",
              self->as.Array.count, self->as.Array.count, (char*)buffer));
      }
    }break;
  }
}

void main_menu(void* ctx){
  App* app = ctx;
  
  Rectangle rect = {
    0,0,
    GetScreenWidth(),GetScreenHeight()
  };

  static Nob_String_Builder data = {0};
  if(data.count == 0){
    nob_read_entire_file("test.data", &data);
  }

  Split split = rect_split(rect, .horizontal=0.5);

  int padding = 2;
  int row_height = style.text.size+padding*3;
  int rows = split.left.height/row_height;
  int cols = 1;
  static Type* view_structure = NULL;
  static Rectangle view_rect = {0};
  static size_t view_offset = 0;
  for(size_t i = 0; i < data.count; ++i){
    Rectangle byte_rect = rect_table_cell(split.left, cols, rows, 0, i);
    DrawRectangleRec(byte_rect, GRAY);
    byte_rect = rect_offset(byte_rect, -1);
    DrawRectangleRec(byte_rect, style.background);
    byte_rect = rect_offset(byte_rect, -padding);
    if(button(byte_rect, nob_temp_sprintf("%02X", data.items[i]), .align = Align_LEFT)
        && dragging_type != NULL
    ){
      view_structure = dragging_type;
      view_rect = dragging_rect;
      view_rect.x = byte_rect.x;
      view_rect.y = byte_rect.y;
      view_offset = i;
      nob_log(NOB_INFO, "view: x = %f, y = %f, w = %f, h = %f",
          view_rect.x, view_rect.y, view_rect.width, view_rect.height);
    }

  }
  if(view_structure != NULL){
    Type_render(rect_table_cell(split.left, cols, rows, 0, view_offset),view_structure, data.items+view_offset);
  }

  // struct menu
  
  rect = rect_offset(split.right, -padding);
  DrawRectangleRec(rect, DARKGRAY);
  rect = rect_offset(rect, -padding);

  static Type base_struct = {
    .kind = Type_STRUCT,
    .as.Struct = {0},
  };

  Type_render(rect, &base_struct, NULL);  
  
  if(dragging_type != NULL){
    dragging_rect.x = GetMouseX();
    dragging_rect.y = GetMouseY();
    nob_log(NOB_INFO, "x = %f, y = %f, w = %f, h = %f",
        dragging_rect.x, dragging_rect.y, dragging_rect.width, dragging_rect.height);
    Type_render(dragging_rect, dragging_type, NULL);
  }
}

void* nhl_init(int argc, char **argv){
  App* app = calloc(1,sizeof(App));

  TestData test_data = {
    .a = 12,
    .b = 42,
    .str = "Hello!"
  };

  nob_write_entire_file("test.data", &test_data, sizeof(TestData));

  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(800, 600, "binspector");
  SetTargetFPS(60);
  return app;
}

bool nhl_update(void* ctx){
  BeginDrawing();
    ClearBackground(GRAY);
    main_menu(ctx);
  EndDrawing();
  nob_temp_reset();
  if(!mouse_cursor_set){
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);
  }
  mouse_cursor_set = false;
  return !WindowShouldClose();
}

void nhl_pre_reload(void* ctx){
  
}

void nhl_post_reload(void* ctx){
  
}

void nhl_destroy(void* ctx){

}

