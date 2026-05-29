#include <QApplication>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QPainter>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QRandomGenerator>
#include <QImage>
#include <QRectF>
#include <vlc/vlc.h>
#include <mutex>
#include <unordered_map>
#include <vector>       
#include <string>
#include <cstring>
#include <iostream>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

typedef enum { VAL_NIL, VAL_NUM, VAL_BOOL, VAL_STR, VAL_NATIVE, VAL_FUNC, VAL_MODULE } ValType;
typedef struct Value {
    ValType type;
    union { double num; bool boolean; char* str; void* native; void* func; void* module_handle; } as;
} Value;

Value make_num(double n) { Value v; v.type = VAL_NUM; v.as.num = n; return v; }
Value make_bool(bool b) { Value v; v.type = VAL_BOOL; v.as.boolean = b; return v; }
Value make_nil() { Value v; v.type = VAL_NIL; return v; }

Value make_str(const char* s) { 
    Value v; 
    v.type = VAL_STR; 
    v.as.str = strdup(s);
    return v; 
}

enum CmdType { CMD_RECT, CMD_TEXT };

struct DrawCmd {
    CmdType type;
    float x, y, w, h, r, g, b;
    std::string text;
};

struct Vertex3D { float x, y, z; };
struct Model3D {
    std::vector<Vertex3D> vertices;
    std::vector<unsigned int> indices;
};

static std::unordered_map<std::string, Model3D> modelRegistry;

struct DrawCmd3D {
    std::string model_id;
    float x, y, z;
    float scale;
};

class GameWindow : public QOpenGLWidget, protected QOpenGLFunctions {
public:
    std::vector<DrawCmd> drawCommands2D;
    std::vector<DrawCmd3D> drawCommands3D;
    std::unordered_map<int, bool> keyStates;
    std::unordered_map<int, bool> mouseStates;
    int mouseX = 0;
    int mouseY = 0;
    int scrollDelta = 0;
    QImage bgImage;
    std::vector<QRectF> collisionBoxes;
    float bg_x = 0.0f;
    float bg_y = 0.0f;
    float bg_w = 0.0f;
    float bg_h = 0.0f;

    libvlc_instance_t* vlc_inst = nullptr;
    libvlc_media_player_t* vlc_mp_audio = nullptr;
    libvlc_media_player_t* vlc_mp_video = nullptr;
    unsigned char* video_pixels = nullptr;
    std::mutex video_mutex;
    float video_x = 0.0f;
    float video_y = 0.0f;
    float video_w = 0.0f;
    float video_h = 0.0f;
    unsigned int vid_width = 0;
    unsigned int vid_height = 0;
    
    float cameraAngle = 0.0f;

    GameWindow(const char* title, int w, int h) {
        setWindowTitle(title);
        setFixedSize(w, h);
        setMouseTracking(true);
    }

    bool isKeyPressed(int qtKey) { return keyStates[qtKey]; }

protected:
    void initializeGL() override {
        initializeOpenGLFunctions();
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    }

    void resizeGL(int w, int h) override {
        glViewport(0, 0, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = (float)w / (float)(h ? h : 1);
        float fH = tan(45.0f / 360.0f * 3.14159f) * 0.1f;
        float fW = fH * aspect;
        glFrustum(-fW, fW, -fH, fH, 0.1f, 100.0f);
        glMatrixMode(GL_MODELVIEW);
    }

    void paintGL() override {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glLoadIdentity();
        glTranslatef(0.0f, -1.0f, -5.0f);
        cameraAngle += 1.0f;

        glEnableClientState(GL_VERTEX_ARRAY);

        for (const auto& cmd : drawCommands3D) {
            if (modelRegistry.find(cmd.model_id) != modelRegistry.end()) {
                Model3D& model = modelRegistry[cmd.model_id];
                
                glPushMatrix();
                glTranslatef(cmd.x, cmd.y, cmd.z);
                glScalef(cmd.scale, cmd.scale, cmd.scale);
                glRotatef(cameraAngle, 0.0f, 1.0f, 0.0f);

                glVertexPointer(3, GL_FLOAT, 0, model.vertices.data());
                
                glColor3f(0.0f, 1.0f, 0.0f); 
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glDrawElements(GL_TRIANGLES, model.indices.size(), GL_UNSIGNED_INT, model.indices.data());
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                
                glPopMatrix();
            }
        }
        glDisableClientState(GL_VERTEX_ARRAY);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QFont font = painter.font(); font.setPointSize(16); font.setBold(true); painter.setFont(font);
        if (!bgImage.isNull()) {
            painter.drawImage(QRectF(bg_x, bg_y, bg_w, bg_h), bgImage);
        }
        
        video_mutex.lock();
        if (video_pixels && vid_width > 0 && vid_height > 0) {
            QImage vidImg(video_pixels, vid_width, vid_height, QImage::Format_RGB32);
            painter.drawImage(QRectF(video_x, video_y, video_w, video_h), vidImg);
        }
        video_mutex.unlock();
        
        for (const auto& cmd : drawCommands2D) {
            if (cmd.type == CMD_RECT) {
                painter.fillRect(QRectF(cmd.x, cmd.y, cmd.w, cmd.h), QColor::fromRgbF(cmd.r, cmd.g, cmd.b));
            } else if (cmd.type == CMD_TEXT) {
                painter.setPen(QColor::fromRgbF(cmd.r, cmd.g, cmd.b));
                painter.drawText(cmd.x, cmd.y, QString::fromUtf8(cmd.text.c_str()));
            }
        }
        painter.end();
    }

    void keyPressEvent(QKeyEvent *event) override { keyStates[event->key()] = true; }
    void keyReleaseEvent(QKeyEvent *event) override { if (!event->isAutoRepeat()) keyStates[event->key()] = false; }
    void mousePressEvent(QMouseEvent *event) override { 
        mouseStates[event->button()] = true; 
    }
    void mouseReleaseEvent(QMouseEvent *event) override { 
        mouseStates[event->button()] = false; 
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        mouseX = event->pos().x();
        mouseY = event->pos().y();
    }
    void wheelEvent(QWheelEvent *event) override {
        scrollDelta = event->angleDelta().y();
    }
};

static void* vlc_video_lock(void* data, void** p_pixels) {
    GameWindow* gw = static_cast<GameWindow*>(data);
    gw->video_mutex.lock();
    *p_pixels = gw->video_pixels;
    return nullptr;
}

static void vlc_video_unlock(void* data, void* id, void* const* p_pixels) {
    GameWindow* gw = static_cast<GameWindow*>(data);
    gw->video_mutex.unlock();
}

static void vlc_video_display(void* data, void* id) {
}

static unsigned vlc_video_format(void** data, char* chroma, unsigned* width, unsigned* height, unsigned* pitches, unsigned* lines) {
    GameWindow* gw = static_cast<GameWindow*>(*data);
    memcpy(chroma, "RV32", 4);
    gw->vid_width = *width;
    gw->vid_height = *height;
    if (gw->video_pixels) {
        delete[] gw->video_pixels;
    }
    gw->video_pixels = new unsigned char[*width * *height * 4];
    *pitches = *width * 4;
    *lines = *height;
    return 1;
}

static void vlc_video_cleanup(void* data) {
    GameWindow* gw = static_cast<GameWindow*>(data);
    if (gw->video_pixels) {
        delete[] gw->video_pixels;
        gw->video_pixels = nullptr;
    }
}
static QApplication *app = nullptr;
static GameWindow *window = nullptr;

int string_to_qt_key(const char* keyName) {
    std::string k = keyName;
    for(auto& c : k) c = toupper(c);
    if (k == "SPACE") return Qt::Key_Space;
    if (k == "UP") return Qt::Key_Up;
    if (k == "DOWN") return Qt::Key_Down;
    if (k == "LEFT") return Qt::Key_Left;
    if (k == "RIGHT") return Qt::Key_Right;
    if (k == "ESC" || k == "ESCAPE") return Qt::Key_Escape;
    if (k == "ENTER" || k == "RETURN") return Qt::Key_Return;
    if (k == "SHIFT") return Qt::Key_Shift;
    if (k == "CTRL" || k == "CONTROL") return Qt::Key_Control;
    if (k == "ALT") return Qt::Key_Alt;
    if (k.length() == 1) return k[0];
    return 0;
}

#ifdef _WIN32
#define VL_EXPORT __declspec(dllexport)
#else
#define VL_EXPORT
#endif

extern "C" {

    VL_EXPORT Value init_window(int argCount, Value* args) {
        if (argCount < 2) return make_nil();
        
        const char* title = "VerseLanguage Game Engine";
        int w = 800;
        int h = 600;

        if (args[0].type == VAL_STR && argCount >= 3) {
            title = args[0].as.str;
            w = (int)args[1].as.num;
            h = (int)args[2].as.num;
        } else if (args[0].type == VAL_NUM && argCount >= 2) {
            w = (int)args[0].as.num;
            h = (int)args[1].as.num;
        } else {
            return make_nil();
        }

        if (!app) {
            static int argc = 1;
            static char name[] = "vl-game";
            static char* argv[] = { name, nullptr };
            app = new QApplication(argc, argv);
        }
        if (!window) {
            window = new GameWindow(title, w, h);
            window->show();
        }
        return make_nil();
    }

    VL_EXPORT Value import_3d(int argCount, Value* args) {
        if (argCount < 1 || args[0].type != VAL_STR) return make_nil();
        std::string filepath = args[0].as.str;

        if (modelRegistry.find(filepath) != modelRegistry.end()) {
            return make_str(filepath.c_str());
        }

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(filepath, 
            aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            std::cerr << "ASSIMP ERROR: Failed to load file " << filepath << " -> " << importer.GetErrorString() << std::endl;
            return make_nil();
        }

        Model3D newModel;
        if (scene->mNumMeshes > 0) {
            aiMesh* mesh = scene->mMeshes[0];
            for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
                Vertex3D v;
                v.x = mesh->mVertices[i].x;
                v.y = mesh->mVertices[i].y;
                v.z = mesh->mVertices[i].z;
                newModel.vertices.push_back(v);
            }
            for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
                aiFace face = mesh->mFaces[i];
                for (unsigned int j = 0; j < face.mNumIndices; j++)
                    newModel.indices.push_back(face.mIndices[j]);
            }
        }

        modelRegistry[filepath] = newModel;
        
        Value result; result.type = VAL_STR; result.as.str = strdup(filepath.c_str());
        return result;
    }

    VL_EXPORT Value draw_3d(int argCount, Value* args) {
        if (argCount < 5 || !window || args[0].type != VAL_STR) return make_nil();
        DrawCmd3D cmd;
        cmd.model_id = args[0].as.str;
        cmd.x = (float)args[1].as.num;
        cmd.y = (float)args[2].as.num;
        cmd.z = (float)args[3].as.num;
        cmd.scale = (float)args[4].as.num;
        
        window->drawCommands3D.push_back(cmd);
        return make_nil();
    }

    VL_EXPORT Value draw_rect(int argCount, Value* args) {
        if (argCount < 7 || !window) return make_nil();
        DrawCmd cmd;
        cmd.type = CMD_RECT;
        cmd.x = (float)args[0].as.num; 
        cmd.y = (float)args[1].as.num;
        cmd.w = (float)args[2].as.num; 
        cmd.h = (float)args[3].as.num;
        cmd.r = (float)args[4].as.num; 
        cmd.g = (float)args[5].as.num; 
        cmd.b = (float)args[6].as.num;
        
        window->drawCommands2D.push_back(cmd);
        return make_nil();
    }

    VL_EXPORT Value draw_text(int argCount, Value* args) {
        if (argCount < 6 || !window || args[0].type != VAL_STR) return make_nil();
        DrawCmd cmd;
        cmd.type = CMD_TEXT;
        cmd.text = args[0].as.str;
        cmd.x = (float)args[1].as.num; 
        cmd.y = (float)args[2].as.num;
        cmd.r = (float)args[3].as.num; 
        cmd.g = (float)args[4].as.num; 
        cmd.b = (float)args[5].as.num;
        
        window->drawCommands2D.push_back(cmd);
        return make_nil();
    }

    VL_EXPORT Value update(int argCount, Value* args) {
        if (!window || !app) return make_bool(false);
        window->update(); 
        app->processEvents(); 
        
        window->drawCommands2D.clear();
        window->drawCommands3D.clear(); 
        window->scrollDelta = 0;
        window->collisionBoxes.clear();
        return make_bool(window->isVisible());
    }

    VL_EXPORT Value is_key_pressed(int argCount, Value* args) {
        if (argCount < 1 || !window || args[0].type != VAL_STR) return make_bool(false);
        int qtKey = string_to_qt_key(args[0].as.str);
        return make_bool(window->isKeyPressed(qtKey));
    }

    VL_EXPORT Value get_random(int argCount, Value* args) {
        if (argCount < 1 || args[0].type != VAL_NUM) return make_num(0);
        int max = (int)args[0].as.num;
        if (max <= 0) return make_num(0);
        return make_num(QRandomGenerator::global()->bounded(max));
    }
    VL_EXPORT Value is_mouse_pressed(int argCount, Value* args) {
        if (argCount < 1 || !window || args[0].type != VAL_STR) return make_bool(false);
        std::string btn = args[0].as.str;
        for(auto& c : btn) c = toupper(c);
        int qtBtn = Qt::NoButton;
        if (btn == "LEFT") qtBtn = Qt::LeftButton;
        else if (btn == "RIGHT") qtBtn = Qt::RightButton;
        else if (btn == "CENTER" || btn == "MIDDLE") qtBtn = Qt::MiddleButton;
        return make_bool(window->mouseStates[qtBtn]);
    }

    VL_EXPORT Value get_mouse_x(int argCount, Value* args) {
        if (!window) return make_num(0);
        return make_num(window->mouseX);
    }

    VL_EXPORT Value get_mouse_y(int argCount, Value* args) {
        if (!window) return make_num(0);
        return make_num(window->mouseY);
    }

    VL_EXPORT Value get_mouse_scroll(int argCount, Value* args) {
        if (!window) return make_num(0);
        return make_num(window->scrollDelta);
    }

    VL_EXPORT Value draw_button(int argCount, Value* args) {
        if (argCount < 11 || !window || args[0].type != VAL_STR) return make_bool(false);
        
        std::string text = args[0].as.str;
        float x = (float)args[1].as.num;
        float y = (float)args[2].as.num;
        float w = (float)args[3].as.num;
        float h = (float)args[4].as.num;
        float r = (float)args[5].as.num;
        float g = (float)args[6].as.num;
        float b = (float)args[7].as.num;
        float mx = (float)args[8].as.num;
        float my = (float)args[9].as.num;
        bool mouse_click = args[10].as.boolean;

        bool is_hover = (mx >= x && mx <= x + w && my >= y && my <= y + h);
        bool is_triggered = is_hover && mouse_click;

        DrawCmd rectCmd;
        rectCmd.type = CMD_RECT;
        rectCmd.x = x; 
        rectCmd.y = y; 
        rectCmd.w = w; 
        rectCmd.h = h;
        
        float hover_r = r + 0.15f; if (hover_r > 1.0f) hover_r = 1.0f;
        float hover_g = g + 0.15f; if (hover_g > 1.0f) hover_g = 1.0f;
        float hover_b = b + 0.15f; if (hover_b > 1.0f) hover_b = 1.0f;
        
        rectCmd.r = is_hover ? hover_r : r;
        rectCmd.g = is_hover ? hover_g : g;
        rectCmd.b = is_hover ? hover_b : b;
        window->drawCommands2D.push_back(rectCmd);

        DrawCmd textCmd;
        textCmd.type = CMD_TEXT;
        textCmd.text = text;
        textCmd.x = x + (w * 0.15f); 
        textCmd.y = y + (h * 0.65f);
        
        float brightness = (r * 0.299f) + (g * 0.587f) + (b * 0.114f);
        textCmd.r = (brightness > 0.5f) ? 0.0f : 1.0f;
        textCmd.g = textCmd.r; 
        textCmd.b = textCmd.r;
        window->drawCommands2D.push_back(textCmd);

        return make_bool(is_triggered);
    }
    VL_EXPORT Value load_background(int argCount, Value* args) {
        if (argCount < 5 || !window || args[0].type != VAL_STR) return make_bool(false);
        if (window->bgImage.load(args[0].as.str)) {
            window->bg_x = (float)args[1].as.num;
            window->bg_y = (float)args[2].as.num;
            window->bg_w = (float)args[3].as.num;
            window->bg_h = (float)args[4].as.num;
            return make_bool(true);
        }
        return make_bool(false);
    }

    VL_EXPORT Value play_music(int argCount, Value* args) {
        if (argCount < 1 || !window || args[0].type != VAL_STR) return make_bool(false);
        if (!window->vlc_inst) {
            window->vlc_inst = libvlc_new(0, nullptr);
        }
        if (window->vlc_mp_audio) {
            libvlc_media_player_stop(window->vlc_mp_audio);
            libvlc_media_player_release(window->vlc_mp_audio);
        }
        libvlc_media_t* m = libvlc_media_new_path(window->vlc_inst, args[0].as.str);
        window->vlc_mp_audio = libvlc_media_player_new_from_media(m);
        libvlc_media_release(m);
        libvlc_media_player_play(window->vlc_mp_audio);
        return make_bool(true);
    }

    VL_EXPORT Value stop_music(int argCount, Value* args) {
        if (!window || !window->vlc_mp_audio) return make_bool(false);
        libvlc_media_player_stop(window->vlc_mp_audio);
        libvlc_media_player_release(window->vlc_mp_audio);
        window->vlc_mp_audio = nullptr;
        return make_bool(true);
    }

    VL_EXPORT Value play_video(int argCount, Value* args) {
        if (argCount < 5 || !window || args[0].type != VAL_STR) return make_bool(false);
        if (!window->vlc_inst) {
            window->vlc_inst = libvlc_new(0, nullptr);
        }
        if (window->vlc_mp_video) {
            libvlc_media_player_stop(window->vlc_mp_video);
            libvlc_media_player_release(window->vlc_mp_video);
        }
        
        window->video_x = (float)args[1].as.num;
        window->video_y = (float)args[2].as.num;
        window->video_w = (float)args[3].as.num;
        window->video_h = (float)args[4].as.num;

        libvlc_media_t* m = libvlc_media_new_path(window->vlc_inst, args[0].as.str);
        window->vlc_mp_video = libvlc_media_player_new_from_media(m);
        libvlc_media_release(m);

        libvlc_video_set_callbacks(window->vlc_mp_video, vlc_video_lock, vlc_video_unlock, vlc_video_display, window);
        libvlc_video_set_format_callbacks(window->vlc_mp_video, vlc_video_format, vlc_video_cleanup);

        libvlc_media_player_play(window->vlc_mp_video);
        return make_bool(true);
    }

    VL_EXPORT Value add_wall(int argCount, Value* args) {
        if (argCount < 4 || !window) return make_nil();
        window->collisionBoxes.push_back(QRectF(
            (float)args[0].as.num, 
            (float)args[1].as.num, 
            (float)args[2].as.num, 
            (float)args[3].as.num
        ));
        return make_nil();
    }

    VL_EXPORT Value check_collision(int argCount, Value* args) {
        if (argCount < 4 || !window) return make_bool(false);
        QRectF player(
            (float)args[0].as.num, 
            (float)args[1].as.num, 
            (float)args[2].as.num, 
            (float)args[3].as.num
        );
        for (const auto& wall : window->collisionBoxes) {
            if (player.intersects(wall)) {
                return make_bool(true);
            }
        }
        return make_bool(false);
    }
}
