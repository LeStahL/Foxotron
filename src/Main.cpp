#include <stdio.h>
#include <algorithm>

#include "Geometry.h"

#define IMGUI_IMPL_OPENGL_LOADER_GLEW
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "ImGuiFileBrowser.h"

#include "ext.hpp"
#include "gtx/rotate_vector.hpp"

struct Shader
{
  const char * mName;
  const char * mVertexShaderPath;
  const char * mFragmentShaderPath;
} gShaders[] = {
  { "Basic SpecGloss", "Shaders/basic_specgloss.vs", "Shaders/basic_specgloss.fs" },
  { NULL, NULL, NULL, },
};

Shader * gCurrentShader = NULL;
bool LoadShader( Shader * shader )
{
  char vertexShader[ 16 * 1024 ] = { 0 };
  FILE * fileVS = fopen( shader->mVertexShaderPath, "rb" );
  if ( !fileVS )
  {
    printf( "Vertex shader load failed: '%s'\n", shader->mVertexShaderPath );
    return false;
  }
  fread( vertexShader, 1, 16 * 1024, fileVS );
  fclose( fileVS );

  char fragmentShader[ 16 * 1024 ] = { 0 };
  FILE * fileFS = fopen( shader->mFragmentShaderPath, "rb" );
  if ( !fileFS )
  {
    printf( "Fragment shader load failed: '%s'\n", shader->mFragmentShaderPath );
    return false;
  }
  fread( fragmentShader, 1, 16 * 1024, fileFS );
  fclose( fileFS );

  char error[ 4096 ];
  if ( !Renderer::ReloadShaders( vertexShader, (int) strlen( vertexShader ), fragmentShader, (int) strlen( fragmentShader ), error, 4096 ) )
  {
    printf( "Shader build failed: %s\n", error );
    return false;
  }

  gCurrentShader = shader;

  return true;
}

glm::vec3 cameraTarget( 0.0f, 0.0f, 0.0f );
float cameraDistance = 500.0f;

bool LoadMesh( const char * path )
{
  if ( !Geometry::LoadMesh( path ) )
  {
    return false;
  }

  cameraTarget = ( Geometry::mAABBMin + Geometry::mAABBMax ) / 2.0f;
  cameraDistance = glm::length( cameraTarget - Geometry::mAABBMin ) * 4.0f;

  return true;
}

int main( int argc, const char * argv[] )
{
  //////////////////////////////////////////////////////////////////////////
  // Init renderer
  RENDERER_SETTINGS settings;
  settings.bVsync = false;
  settings.nWidth = 1280;
  settings.nHeight = 720;
  settings.windowMode = RENDERER_WINDOWMODE_WINDOWED;
  if ( !Renderer::Open( &settings ) )
  {
    printf( "Renderer::Open failed\n" );
    return -1;
  }

  //////////////////////////////////////////////////////////////////////////
  // Start up ImGui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO & io = ImGui::GetIO();

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL( Renderer::mWindow, true );
  ImGui_ImplOpenGL3_Init();

  imgui_addons::ImGuiFileBrowser file_dialog;

  //////////////////////////////////////////////////////////////////////////
  // Bootstrap
  if ( !LoadShader( &gShaders[ 0 ] ) )
  {
    return -4;
  }

  if ( argc >= 2 )
  {
    LoadMesh( argv[ 1 ] );
  }

  //////////////////////////////////////////////////////////////////////////
  // Mainloop
  bool appWantsToQuit = false;
  bool automaticCamera = false;
  glm::mat4x4 viewMatrix;
  glm::mat4x4 projectionMatrix;
  bool movingCamera = false;
  bool movingLight = false;
  float cameraYaw = 0.0f;
  float cameraPitch = 0.0f;
  float lightYaw = 0.0f;
  float lightPitch = 0.0f;
  float mouseClickPosX = 0.0f;
  float mouseClickPosY = 0.0f;
  glm::vec4 clearColor( 0.08f, 0.18f, 0.18f, 1.0f );
  std::string supportedExtensions = Geometry::GetSupportedExtensions();
  while ( !Renderer::WantsToQuit() && !appWantsToQuit )
  {
    Renderer::StartFrame( clearColor );

    //////////////////////////////////////////////////////////////////////////
    // ImGui windows etc.
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    bool openFileDialog = false;
    static bool showModelInfo = false;
    if ( ImGui::BeginMainMenuBar() )
    {
      if ( ImGui::BeginMenu( "File" ) )
      {
        if ( ImGui::MenuItem( "Open model..." ) )
        {
          openFileDialog = true;
        }
        ImGui::Separator();
        if ( ImGui::MenuItem( "Exit" ) )
        {
          appWantsToQuit = true;
        }
        ImGui::EndMenu();
      }
      if ( ImGui::BeginMenu( "View" ) )
      {
        ImGui::MenuItem( "Enable idle camera", NULL, &automaticCamera );
        ImGui::MenuItem( "Show model info", NULL, &showModelInfo );
        ImGui::Separator();
        ImGui::ColorEdit4( "Background", (float *) &clearColor, ImGuiColorEditFlags_AlphaPreviewHalf );
#ifdef _DEBUG
        ImGui::Separator();
        ImGui::DragFloat3( "Camera Target", (float *) &cameraTarget );
#endif
        ImGui::EndMenu();
      }
      if ( ImGui::BeginMenu( "Shaders" ) )
      {
        for ( int i = 0; gShaders[ i ].mName; i++ )
        {
          bool selected = &gShaders[ i ] == gCurrentShader;
          if ( ImGui::MenuItem( gShaders[ i ].mName, NULL, &selected ) )
          {
            LoadShader( &gShaders[ i ] );
          }
        }
        ImGui::EndMenu();
      }

      ImGui::EndMainMenuBar();
    }

    if ( openFileDialog )
    {
      ImGui::OpenPopup( "Open model" );
    }

    if ( file_dialog.showFileDialog( "Open model", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2( 700, 310 ), supportedExtensions.c_str() ) )
    {
      LoadMesh( file_dialog.selected_path.c_str() );
    }

    if ( showModelInfo )
    {
      ImGui::Begin( "Model info", &showModelInfo );
      ImGui::BeginTabBar( "model" );
      if ( ImGui::BeginTabItem( "Summary" ) )
      {
        int triCount = 0;
        for ( std::map<int, Geometry::Mesh>::iterator it = Geometry::mMeshes.begin(); it != Geometry::mMeshes.end(); it++ )
        {
          triCount += it->second.mTriangleCount;
        }

        ImGui::Text( "Triangle count: %d", triCount );
        ImGui::Text( "Mesh count: %d", Geometry::mMeshes.size() );

        ImGui::EndTabItem();
      }
      if ( ImGui::BeginTabItem( "Textures / Materials" ) )
      {
        ImGui::Text( "Material count: %d", Geometry::mMaterials.size() );

        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
      ImGui::End();
    }

    ImGui::Render();

    //////////////////////////////////////////////////////////////////////////
    // Drag'n'drop

    for ( int i = 0; i < Renderer::dropEventBufferCount; i++ )
    {
      std::string & path = Renderer::dropEventBuffer[ i ];
      LoadMesh( path.c_str() );

    }
    Renderer::dropEventBufferCount = 0;

    //////////////////////////////////////////////////////////////////////////
    // Mouse rotation

    if ( !io.WantCaptureMouse )
    {
      for ( int i = 0; i < Renderer::mouseEventBufferCount; i++ )
      {
        Renderer::MouseEvent & mouseEvent = Renderer::mouseEventBuffer[ i ];
        switch ( mouseEvent.eventType )
        {
          case Renderer::MOUSEEVENTTYPE_MOVE:
            {
              const float rotationSpeed = 130.0f;
              if ( movingCamera )
              {
                cameraYaw -= ( mouseEvent.x - mouseClickPosX ) / rotationSpeed;
                cameraPitch += ( mouseEvent.y - mouseClickPosY ) / rotationSpeed;

                // Clamp to avoid gimbal lock
                cameraPitch = std::min( cameraPitch, 1.5f );
                cameraPitch = std::max( cameraPitch, -1.5f );
              }
              if ( movingLight )
              {
                // Light is a direction so it moves "backwards"
                lightYaw += ( mouseEvent.x - mouseClickPosX ) / rotationSpeed;
                lightPitch -= ( mouseEvent.y - mouseClickPosY ) / rotationSpeed;

                // Clamp to avoid gimbal lock
                lightPitch = std::min( lightPitch, 1.5f );
                lightPitch = std::max( lightPitch, -1.5f );
              }
              mouseClickPosX = mouseEvent.x;
              mouseClickPosY = mouseEvent.y;
            }
            break;
          case Renderer::MOUSEEVENTTYPE_DOWN:
            {
              if ( mouseEvent.button == Renderer::MOUSEBUTTON_LEFT )
              {
                movingCamera = true;
                automaticCamera = false;
              }
              else if ( mouseEvent.button == Renderer::MOUSEBUTTON_RIGHT )
              {
                movingLight = true;
              }
              mouseClickPosX = mouseEvent.x;
              mouseClickPosY = mouseEvent.y;
            }
            break;
          case Renderer::MOUSEEVENTTYPE_UP:
            {
              if ( mouseEvent.button == Renderer::MOUSEBUTTON_LEFT )
              {
                movingCamera = false;
              }
              else if ( mouseEvent.button == Renderer::MOUSEBUTTON_RIGHT )
              {
                movingLight = false;
              }
            }
            break;
          case Renderer::MOUSEEVENTTYPE_SCROLL:
            {
              const float aspect = 1.1f;
              cameraDistance *= mouseEvent.y < 0 ? aspect : 1 / aspect;
            }
            break;
        }
      }
    }
    Renderer::mouseEventBufferCount = 0;

    //////////////////////////////////////////////////////////////////////////
    // Mesh render

    if ( automaticCamera )
    {
      cameraYaw += io.DeltaTime * 0.3f;
    }

    float verticalFovInRadian = 0.5f;
    projectionMatrix = glm::perspective( verticalFovInRadian, settings.nWidth / (float) settings.nHeight, 0.01f, cameraDistance * 2.0f );
    Renderer::SetShaderConstant( "mat_projection", projectionMatrix );

    glm::vec3 cameraPosition( 0.0f, 0.0f, -1.0f );
    cameraPosition = glm::rotateX( cameraPosition, cameraPitch );
    cameraPosition = glm::rotateY( cameraPosition, cameraYaw );
    cameraPosition *= cameraDistance;
    Renderer::SetShaderConstant( "camera_position", cameraPosition );

    glm::vec3 lightDirection( 0.0f, 0.0f, -1.0f );
    lightDirection = glm::rotateX( lightDirection, lightPitch );
    lightDirection = glm::rotateY( lightDirection, lightYaw );
    Renderer::SetShaderConstant( "light_direction", lightDirection );

    viewMatrix = glm::lookAtRH( cameraPosition + cameraTarget, cameraTarget, glm::vec3( 0.0f, 1.0f, 0.0f ) );
    Renderer::SetShaderConstant( "mat_view", viewMatrix );

    for ( std::map<int, Geometry::Node>::iterator it = Geometry::mNodes.begin(); it != Geometry::mNodes.end(); it++ )
    {
      const Geometry::Node & node = it->second;

      Renderer::SetShaderConstant( "mat_world", Geometry::mMatrices[ node.mID ] );

      for ( int i = 0; i < it->second.mMeshes.size(); i++ )
      {
        const Geometry::Mesh & mesh = Geometry::mMeshes[ it->second.mMeshes[ i ] ];
        const Geometry::Material & material = Geometry::mMaterials[ mesh.mMaterialIndex ];
        if ( material.mTextureDiffuse )
        {
          Renderer::SetShaderTexture( "tex_diffuse", material.mTextureDiffuse );
        }
        if ( material.mTextureNormals )
        {
          Renderer::SetShaderTexture( "tex_normals", material.mTextureNormals );
        }
        if ( material.mTextureSpecular )
        {
          Renderer::SetShaderTexture( "tex_specular", material.mTextureSpecular );
        }
        if ( material.mTextureAlbedo )
        {
          Renderer::SetShaderTexture( "tex_albedo", material.mTextureAlbedo );
        }
        if ( material.mTextureRoughness )
        {
          Renderer::SetShaderTexture( "tex_roughness", material.mTextureRoughness );
        }
        if ( material.mTextureMetallic )
        {
          Renderer::SetShaderTexture( "tex_metallic", material.mTextureMetallic );
        }

        glBindVertexArray( mesh.mVertexArrayObject );

        glDrawElements( GL_TRIANGLES, mesh.mTriangleCount * 3, GL_UNSIGNED_INT, NULL );
      }
    }

    //////////////////////////////////////////////////////////////////////////
    // End frame
    ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
    Renderer::EndFrame();
  }

  //////////////////////////////////////////////////////////////////////////
  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  Geometry::UnloadMesh();

  Renderer::Close();

  return 0;
}