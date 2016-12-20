//
// Created by David on 4/10/2016.
//

#include "GLScene.h"

GLScene::GLScene(GLOptions options) : _options(options) {
    //One for VColor, one for VPosition
    _buffers = new GLuint[3];
    _matrixLoc = 0;
    _viewerLoc = 0;
    _vNormLoc = 0;
    _vPositionLoc = 0;

    _posx = 0.0;
    _posy = 0.0;
    _thetaY = 0.0;
    _thetaX = 0.0;
    _thetaZ = 0.0;
    _distance = 3;
    _lastx = 0;
    _lastY = 0;
    _lastz = 0;

    _viewer = vec4(0.0, 0.0, _distance, 1.0);

    _lightPos = point4(100, 100.0, 100.0, 1.0);
    _lightAmb = color4(0.2, 0.2, 0.2, 1.0);
    _lightDiff = color4(1.0, 1.0, 1.0, 1.0);
    _lightSpec = color4(1.0, 1.0, 1.0, 1.0);

    _materialAmb = color4(1.0, 1, 1.0, 1.0);
    _materialDiff = color4(1.0, 0.8, 0.0, 1.0);
    _materialSpec = color4(1.0, 0.8, 0.0, 1.0);

    _materialShininess = 100.0;

    _numVertices = 0;

    _points = 0;
    _norms = 0;

    _fovx = 70;
    _fovy = 60;
    _aspect= 1.20754;
    _zNear = 0.5;
    _zFar = 4.5;
    _rot = RotateRandom();
    _rpy = RotateToEuler(_rot);

    _depth_images_count = 0;
    _total_depth_images= _options._numCloudsToRender;

    _ctm = identity();
}

void GLScene::constructTransformMatrix() {

    _rot = RotateRandom();
    _rpy = RotateToEuler(_rot);
    _thetaX = _rpy[0] * 180.0 / M_PI;
    _thetaY = _rpy[1] * 180.0 / M_PI;
    _thetaZ = _rpy[2] * 180.0 / M_PI;

    float phi = _thetaX;// * M_PI/180.0;
    float theta = _thetaY;// * M_PI/180.0;
    float eta = _thetaZ;

    _viewer = vec4(0, 0, _distance, 1);
    vec4 center = vec4(0,0,0);

    //Construct the up vector
    vec4 viewer_to_center = _viewer - center;
    vec4 right = normalize(cross(viewer_to_center, RotateZ(eta) * vec3(0,1,0)));
    vec4 up = normalize(cross(right, viewer_to_center));

    //Rotate the camera's position around the center
    _viewer = (RotateX(theta) * (_viewer - center));
    _viewer = (RotateY(phi) * (_viewer - center));
    _viewer = (RotateZ(eta) * (_viewer - center));

    //Update the transform matrix
    _ctm = identity();
    _ctm *= Perspective(_fovy, _aspect, _zNear, _zFar);
    _ctm *= LookAt(_viewer, center, up);
}

GLScene::~GLScene() {
    delete [] _buffers;
    if(_points)
        delete [] _points;
    if(_norms)
        delete [] _norms;
}

void GLScene::load() {
    GLuint vao;

    glGenVertexArrays( 1, &vao );   // give us 1 VAO:
    glBindVertexArray( vao );       // make it active

    glGenBuffers(1, _buffers);
    glBindBuffer(GL_ARRAY_BUFFER, _buffers[0]);  // make it active

    updateSurfaces();
    if(_options._verbose)
        cout << "Updated surfaces" << endl;
    reloadSurfaces();

}

void GLScene::loadShaders() {
    GLuint program; // name of the shader program

    program = InitShader("/usr/include/depth_renderer/vshader_passthrough.glsl",
       "/usr/include/depth_renderer/fshader_passthrough.glsl");
    glUseProgram(program);

    _vPositionLoc = glGetAttribLocation(program, "vPosition");
    glEnableVertexAttribArray(_vPositionLoc);

    _vNormLoc = glGetAttribLocation(program, "vNorm");
    glEnableVertexAttribArray(_vNormLoc);

    //Set vposition attribute
    glVertexAttribPointer(_vPositionLoc, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
    //Set vnorm attribute
    glVertexAttribPointer(_vNormLoc, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(_numVertices * sizeof(point4)));

    //Handle all uniform variables
    GLint   light_loc = glGetUniformLocation(program, "lightpos"),
    lamb_loc = glGetUniformLocation(program, "light_amb"),
    ldiff_loc = glGetUniformLocation(program, "light_diff"),
    lspec_loc = glGetUniformLocation(program, "light_spec"),
    mamb_loc = glGetUniformLocation(program, "mat_amb"),
    mdiff_loc = glGetUniformLocation(program, "mat_diff"),
    mspec_loc = glGetUniformLocation(program, "mat_spec"),
    phong_loc = glGetUniformLocation(program, "phong");

    _matrixLoc = glGetUniformLocation(program, "ctm");
    _viewerLoc = glGetUniformLocation(program, "viewer");

    glUniformMatrix4fv(_matrixLoc, 1, GL_TRUE, _ctm);
    glUniform4f(_viewerLoc, _viewer[0], _viewer[1], _viewer[2], _viewer[3]);
    glUniform4f(light_loc, _lightPos[0], _lightPos[1], _lightPos[2], _lightPos[3]);
    glUniform4f(lamb_loc, _lightAmb[0], _lightAmb[1], _lightAmb[2], _lightAmb[3]);
    glUniform4f(ldiff_loc, _lightDiff[0], _lightDiff[1], _lightDiff[2], _lightDiff[3]);
    glUniform4f(lspec_loc, _lightSpec[0], _lightSpec[1], _lightSpec[2], _lightSpec[3]);
    glUniform4f(mamb_loc, _materialAmb[0], _materialAmb[1], _materialAmb[2], _materialAmb[3]);
    glUniform4f(mdiff_loc, _materialDiff[0], _materialDiff[1], _materialDiff[2], _materialDiff[3]);
    glUniform4f(mspec_loc, _materialSpec[0], _materialSpec[1], _materialSpec[2], _materialSpec[3]);
    glUniform1f(phong_loc, _materialShininess);
}

void GLScene::loadFromFile() {
    GLParser parser;
    parser.parse(_options._filename, *this);
}

void GLScene::update() {
    constructTransformMatrix();
}

vec3 calcNormal(vec3 &v1, vec3 &v2, vec3 &v3) {
    vec3 V = v2 - v1;
    vec3 W = v3 - v1;
    return normalize(cross(V, W));
}

void GLScene::handleObjs(std::vector<vec3> &vertices, std::vector<vec3> &norms, std::vector<vec3> &faces) {
    faces.resize(_faces.size());
    norms.resize(_vertices.size());
    vertices.resize(_vertices.size());
    for(size_t i = 0; i < _faces.size(); ++i) {
        faces[i] = _faces[i];
    }

    for(size_t i = 0; i < _vertices.size(); ++i) {
        vertices[i] = _vertices[i];
    }

    for(size_t i = 0; i < norms.size(); ++i) {
        norms[i] = vec3();
    }

    for(auto face : faces) {
        vec3 &v1 = vertices[face.x],
        &v2 = vertices[face.y],
        &v3 = vertices[face.z];

        vec3 norm = calcNormal(v1, v2, v3);
        norms[(int) face.x] += norm;
        norms[(int) face.y] += norm;
        norms[(int) face.z] += norm;
    }

    for(size_t i = 0; i < norms.size(); ++i) {
        norms[i] = normalize(norms[i]);
    }
}

void GLScene::assignPointsFromSurfaces() {
    std::vector<vec3> faces;
    std::vector<vec3> vertices;
    std::vector<vec3> norms;

    _numVertices = 0;


    if(_options._verbose) cout << "Calculating obj surface" << endl;
    handleObjs(vertices, norms, faces);

    float max_x = 0, max_y = 0;
    float min_x = 0, min_y = 0;
    for(auto vertex : vertices) {
        max_x = max(max_x, vertex.x);
        max_y = max(max_y, vertex.y);
        min_x = min(min_x, vertex.x);
        min_y = min(min_y, vertex.y);
    }

    if(_options._verbose) {
        cout << "Vertices" << endl;
        for(auto vertex : vertices)
            cout << vertex << endl;

        cout << "Faces" << endl;
        for(auto face : faces)
            cout << face << endl;

        cout << "Norms" << endl;
        for(auto norm : norms)
            cout << norm << endl;

        cout << "Finished calculating surfaces: " << faces.size() << " " << vertices.size() << endl;
    }

    _numVertices = faces.size() * 3;

    if(_points)
        delete [] _points;
    _points = new point4[_numVertices];
    if(_norms)
        delete [] _norms;
    _norms = new vec4[_numVertices];

    if(_options._verbose)
        cout << "Loading values" << endl;

    for(size_t i = 0; i < faces.size(); i++) {
        _points[3*i+0] = vertices[(int) faces[i].x];
        _points[3*i+1] = vertices[(int) faces[i].y];
        _points[3*i+2] = vertices[(int) faces[i].z];

        _norms[3*i+0] = norms[(int) faces[i].x];
        _norms[3*i+1] = norms[(int) faces[i].y];
        _norms[3*i+2] = norms[(int) faces[i].z];

        if(_options._verbose) {
            cout << "face " << i << ":" << endl;
            cout << norms[(int) faces[i].x] << endl;
            cout << norms[(int) faces[i].y] << endl;
            cout << norms[(int) faces[i].z] << endl;
            cout << vertices[(int) faces[i].x] << endl;
            cout << vertices[(int) faces[i].y] << endl;
            cout << vertices[(int) faces[i].z] << endl;
        }
    }
    if(_options._verbose)
        cout << "Finished loading normals" << endl;
}

int GLScene::screenShot(int const num) {
        // we will store the image data here
    float *pixels;
        // the thingy we use to write files        // we get the width/height of the screen into this array
    int screenStats[4];

        // get the width/height of the window
    glGetIntegerv(GL_VIEWPORT, screenStats);

        // generate an array large enough to hold the pixel data 
        // (width*height*bytesPerPixel)
    pixels = new float[screenStats[2]*screenStats[3]];
        // read in the pixel data, TGA's pixels are BGR aligned
    glReadPixels(0, 0, screenStats[2], screenStats[3], GL_DEPTH_COMPONENT, GL_FLOAT, pixels);

    const std::size_t image_width( screenStats[2] );
    const std::size_t image_height( screenStats[3] );

    int cloud_size = 0;
    for ( std::size_t y(0); y != image_height; ++y ) {
      for ( std::size_t x(0); x != image_width; ++x ) {
        float * rgb( pixels + (y * image_width + x) );
        bool valid_point = rgb[0] !=  1 && rgb[0] !=  0;
        if (valid_point) {
            cloud_size++;
        }
    }
}

pcl::PointCloud<pcl::PointXYZ> cloud;
cloud.width = cloud_size;
cloud.height   = 1;
cloud.is_dense = false;
cloud.points.resize (cloud.width * cloud.height);

    // float fov_x_deg = 70;
    // float fov_y_deg = 60;
    // focal_length_x = 256 / (tan(fov_x / 2))
    // focal_length_y = 212 / (tan(fov_y / 2))
float focal_length_x = 365.605889725;
float focal_length_y = 367.194771204; 

int i = 0;
for ( std::size_t v(0); v != image_height; ++v )
{
  for ( std::size_t u(0); u != image_width; ++u )
  {
    float * rgb( pixels + (v * image_width + u) );
    float clipped_depth_val = rgb[0];

    bool valid_point = clipped_depth_val !=  1 && clipped_depth_val !=  0;

    if ( valid_point){
        float z = clipped_depth_val * (_zFar - _zNear) +  _zNear;
                cloud.points[i].x = (u-256.0)/focal_length_x  * z / _distance;   //WHy _distance?
                cloud.points[i].y = (v-212.0)/focal_length_y  * z / _distance;   //WHy _distance?
                cloud.points[i].z = z;
                i++;

            }
        }
    }
    std::stringstream ss_pcd;
    ss_pcd << _options._output_dir;
    ss_pcd << _depth_images_count ;
    ss_pcd << ".pcd";

    pcl::io::savePCDFileBinary(ss_pcd.str().c_str(), cloud);

    std::stringstream ss_info;
    ss_info << _options._output_dir;
    ss_info << "info" ;
    ss_info << ".csv";

    std::stringstream ss_pose;
    ss_pose << _options._output_dir;
    ss_pose << "pose" ;
    ss_pose << ".csv";

    std::ofstream posefile;
    std::ofstream infofile;

    if (_depth_images_count == 0)
    {
        //open without append
        posefile.open(ss_pose.str().c_str());
        infofile.open(ss_info.str().c_str());
        posefile << "id, ";
        posefile << "R00, R01, R02, R10, R11, R12, R20, R21, R22, ";
        posefile << "roll, pitch, yaw" << std::endl;;

        infofile << "distance, ";
        infofile << "fx, fy, ";
        infofile << "fov_x_deg, fov_y_deg, ";
        infofile << "xdim, ydim, ";
        infofile << "aspect, zNear, zFar " << std::endl;

        infofile << _distance << ", ";
        infofile <<  focal_length_x << ", "  << focal_length_x << ", ";
        infofile << 70 << ", "  << 60 << ", ";
        infofile << 512 << ", "  << 424 << ", ";
        infofile << _aspect << ", "  << _zNear << ", " <<  _zFar << std::endl;
        infofile.close();
    }
    else{
        //Open to append
        posefile.open(ss_pose.str().c_str(), std::ios_base::app);
        infofile.open(ss_info.str().c_str(), std::ios_base::app);
    }

    // myfile << "id, "
    posefile << _depth_images_count  << ", ";
    // myfile << "R00, R01, R02,R10, R11, R12,R20, R21, R22, "
    posefile << _rot[0][0]  << ", ";
    posefile << _rot[0][1]  << ", ";
    posefile << _rot[0][2]  << ", ";
    posefile << _rot[1][0]  << ", ";
    posefile << _rot[1][1]  << ", ";
    posefile << _rot[1][2]  << ", ";
    posefile << _rot[2][0]  << ", ";
    posefile << _rot[2][1]  << ", ";
    posefile << _rot[2][2]  << ", ";
    // myfile << "roll, pitch, yaw, ";
    posefile << _rpy[0] << ", ";
    posefile << _rpy[0]  << ", ";
    posefile << _rpy[0]  << std::endl;;

    posefile.close();

    _depth_images_count++;

    if (_depth_images_count  ==_total_depth_images )
    {
        exit(EXIT_SUCCESS);
    }

    // free the memory
    delete [] pixels;
    // return success
    return 0;
}

void GLScene::updateSurfaces() {
    assignPointsFromSurfaces();
}

void GLScene::reloadSurfaces() {
    if(_options._verbose)
        cout << "Buffering data" << endl;

    //Set vposition attribute
    glVertexAttribPointer(_vPositionLoc, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0));
    //Set vnorm attribute
    glVertexAttribPointer(_vNormLoc, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(_numVertices * sizeof(point4)));

    size_t size = _numVertices * (sizeof(point4) + sizeof(vec4) + sizeof(vec2));
    glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_STATIC_DRAW);
    if(_options._verbose)
        cout << "Buffered data " << size << endl;

    glBufferSubData( GL_ARRAY_BUFFER, 0, _numVertices * sizeof(point4), _points );
    if(_options._verbose)
        cout << "Buffered points " << _numVertices * sizeof(point4) << endl;
    glBufferSubData( GL_ARRAY_BUFFER, _numVertices * sizeof(point4), _numVertices * sizeof(vec4), _norms );
    if(_options._verbose)
        cout << "Buffered norms " << _numVertices * sizeof(vec4) << endl;
}

void GLScene::reload() {
    glUniformMatrix4fv(_matrixLoc, 1, GL_FALSE, _ctm);
    glUniform4f(_viewerLoc, _viewer[0], _viewer[1], _viewer[2], _viewer[3]);

    glDrawArrays(GL_TRIANGLES, 0, _numVertices);
}

//ACCESSORS/MUTATORS

void GLScene::setThetaX(float thetaX) {
    _thetaX = thetaX;
}

void GLScene::setThetaY(float thetaY) {
    _thetaY = thetaY;
}

void GLScene::setThetaZ(float thetaZ) {
    _thetaZ = thetaZ;
}

void GLScene::setPosX(float posx) {
    _posx = posx;
}

void GLScene::setPosY(float posy) {
    _posy = posy;
}

void GLScene::setLastX(int lastx) {
    _lastx = lastx;
}

void GLScene::setLastZ(int lastz) {
    _lastz = lastz;
}

float GLScene::getPosX() {
    return _posx;
}

float GLScene::getPosY() {
    return _posy;
}

int GLScene::getLastX() {
    return _lastx;
}

int GLScene::getLastZ() {
    return _lastz;
}

float GLScene::getThetaX() {
    return _thetaX;
}

float GLScene::getThetaY() {
    return _thetaY;
}

float GLScene::getThetaZ() {
    return _thetaZ;
}

void GLScene::setLastY(int y) {
    _lastY = y;
}

void GLScene::decrementDistance() {
    --_distance;
    if(_distance < 2)
        _distance = 2;
}

void GLScene::incrementDistance() {
    ++_distance;
    if(_distance > 50)
        _distance = 50;
}

int GLScene::getLastY() {
    return _lastY;
}

void GLScene::resetDistance() {
    _distance = 2;
}

void GLScene::addFace(int v1, int v2, int v3) {
    //std::cout << "f: " << v1 << " " << v2 << " " << v3 << std::endl; 
    int size = _vertices.size();
    if (v1 >= 0 &&  v2 >= 0 &&  v3 >= 0 && v1 <= size && v2 <= size && v3 <= size) {
        _faces.push_back(face(v1, v2, v3));
    } else {
        cerr << "Unable to find vertices: " << v1 << " " << v2 << " " << v3 << endl;
    }
}

void GLScene::addVertex(float p1, float p2, float p3) {
    //std::cout << "v: " << p1 << " " << p2 << " " << p3 << std::endl; 
    _vertices.push_back(vertex(p1, p2, p3));
}

void GLScene::renderOffScreen() {
    // // create a render buffer for the depth-buffer. this is needed for the framebuffer to depth test when rendering
    // GLuint _multisamplingDepthRenderbuffer;
    // glGenRenderbuffers(1, &_multisamplingDepthRenderbuffer);
    // glBindRenderbuffer(GL_RENDERBUFFER, _multisamplingDepthRenderbuffer);
    // glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _multisamplingDepthRenderbuffer);
    
    // // Create framebuffer for resolving the image
    // GLuint _resolveFrameBuffer;
    // glGenFramebuffers(1, &_resolveFrameBuffer);
    // glBindFramebuffer(GL_FRAMEBUFFER, _resolveFrameBuffer);

    // // Create a renderbuffer object. A renderbuffer is optimized to store images.
    // // https://www.opengl.org/wiki/Renderbuffer_Object
    // GLuint _resolveColorRenderbuffer;
    // glGenRenderbuffers(1, &_resolveColorRenderbuffer);
    // glBindRenderbuffer(GL_RENDERBUFFER, _resolveColorRenderbuffer);
    // // glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB8_OES, 525, 424);
    // //  attach the renderbuffer object to the framebuffer object
    // glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _resolveColorRenderbuffer);

    // create a renderbuffer object to store depth info
    GLuint rboId;
    glGenRenderbuffers(1, &rboId);
    glBindRenderbuffer(GL_RENDERBUFFER, rboId);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 525, 424);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    // create a framebuffer object
    GLuint fboId;
    glGenFramebuffers(1, &fboId);
    glBindFramebuffer(GL_FRAMEBUFFER, fboId);

    // attach the renderbuffer to depth attachment point
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,      // 1. fbo target: GL_FRAMEBUFFER
                              GL_DEPTH_ATTACHMENT, // 2. attachment point
                              GL_RENDERBUFFER,     // 3. rbo target: GL_RENDERBUFFER
                              rboId);              // 4. rbo ID

    load();
    loadShaders();

    // set the background color (white)
    glClearColor(1.0, 1.0, 1.0, 1.0);
    glEnable(GL_DEPTH_TEST);

    for(int i = 0; i < _options._numCloudsToRender; ++i) {
        update();
        reload();
        glBindFramebuffer(GL_FRAMEBUFFER, fboId);
        glViewport(0, 0, 525, 424);

        // glBindFramebuffer(GL_FRAMEBUFFER, rboId);
        // glPixelStorei(GL_PACK_ALIGNMENT, 4);
        screenShot(0);
        glClearColor(1.0, 1.0, 1.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}