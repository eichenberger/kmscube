/*
 * Copyright (c) 2017 Rob Clark <rclark@redhat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include "common.h"
#include "esUtil.h"

static struct {
	struct egl egl;

	GLfloat aspect;

	GLuint program;
	GLint modelviewmatrix, modelviewprojectionmatrix, normalmatrix;
	GLuint vbo;
	GLuint positionsoffset, colorsoffset, normalsoffset;
} gl;

// Vertex shader
const GLchar* vertexShaderSrc = "\
    #version 320 es\n\
\
    in vec4 inValue;\n\
    out vec4 outValue;\n\
\
    void main()\n\
    {\n\
        outValue = inValue*vec4(1,2,3,4);\
	for(int i=0;i<100;++i) { \
	    outValue = mat4(vec4(1,2,3,4), \
			    vec4(5,6,7,8), \
			    vec4(9,10,11,12), \
			    vec4(13,14,15,16))*outValue;\n \
	} \
    }\n\
";

static const char *fragmentShaderSource=
		"#version 320 es\n"
		"                                   \n"
		"void main()                        \n"
		"{                                  \n"
		"}                                  \n";

void test(const struct gbm *gbm, int samples)
{
    int ret;

    ret = init_egl(&gl.egl, gbm, samples);
    if (ret)
	return;

    gl.aspect = (GLfloat)(gbm->height) / (GLfloat)(gbm->width);

    // Compile shader
    GLuint shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(shader, 1, &vertexShaderSrc, 0);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &ret);
    if (!ret) {
	    char *log;

	    printf("vertex shader compilation failed!:\n");
	    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &ret);
	    if (ret > 1) {
		    log = malloc(ret);
		    glGetShaderInfoLog(shader, ret, NULL, log);
		    printf("%s", log);
		    free(log);
	    }

	    return;
    }

    GLuint f_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f_shader, 1, &fragmentShaderSource, 0);
    glCompileShader(f_shader);

    glGetShaderiv(f_shader, GL_COMPILE_STATUS, &ret);
    if (!ret) {
	    char *log;

	    printf("fragment shader compilation failed!:\n");
	    glGetShaderiv(f_shader, GL_INFO_LOG_LENGTH, &ret);
	    if (ret > 1) {
		    log = malloc(ret);
		    glGetShaderInfoLog(f_shader, ret, NULL, log);
		    printf("%s", log);
		    free(log);
	    }

	    return;
    }


    // Create program and specify transform feedback variables
    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glAttachShader(program, f_shader);

    const GLchar* feedbackVaryings[] = { "outValue" };
    glTransformFeedbackVaryings(program, 1, feedbackVaryings, GL_INTERLEAVED_ATTRIBS);

    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &ret);
    if (!ret) {
	    char *log;

	    printf("program linking failed:\n");
	    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &ret);

	    if (ret > 1) {
		    log = malloc(ret);
		    glGetProgramInfoLog(program, ret, NULL, log);
		    printf("%s", log);
		    free(log);
	    }

	    return;
    }

    glUseProgram(program);

    // Create VAO
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

#define VEC_COUNT 102400
    // Create input VBO and vertex format
    GLfloat data[VEC_COUNT*4];

    for (unsigned int i = 0; i < 4; i++) {
	for (unsigned int j = 0; j < VEC_COUNT; j++) {
	    data[i+4*j] = j;
	}
    }

    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);

    GLint inputAttrib = glGetAttribLocation(program, "inValue");
    glEnableVertexAttribArray(inputAttrib);
    glVertexAttribPointer(inputAttrib, 4, GL_FLOAT, GL_FALSE, 0, 0);

    // Create transform feedback buffer
    GLuint tbo;
    glGenBuffers(1, &tbo);
    glBindBuffer(GL_ARRAY_BUFFER, tbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), 0, GL_STATIC_READ);

    struct timeval start;
    gettimeofday(&start,NULL);

    clock_t start_clock = clock();

    for (unsigned int i = 0; i < 1000; i++) {
	// Perform feedback transform
	glEnable(GL_RASTERIZER_DISCARD);

	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, tbo);

	glBeginTransformFeedback(GL_POINTS);

	glDrawArrays(GL_POINTS, 0, VEC_COUNT);

	glEndTransformFeedback();

	glDisable(GL_RASTERIZER_DISCARD);

	glFlush();
    }

    struct timeval end;
    gettimeofday(&end,NULL);

    clock_t end_clock = clock();

    // Fetch and print results
    GLfloat *feedback = glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0, sizeof(data), GL_MAP_READ_BIT);

    printf("time: %fs\n", (double)((end.tv_sec*1000000+end.tv_usec)-(start.tv_sec*1000000+start.tv_usec))/1000000);
    printf("CPU usage: %fs\n", (double)(end_clock-start_clock)/CLOCKS_PER_SEC);

#if 1
    (void) feedback;
#else
    for (unsigned int i = 0; i < VEC_COUNT; i++) {
	printf("%f %f %f %f: %f %f %f %f\n",
		data[0+4*i], data[1+4*i],
		data[2+4*i], data[3+4*i],
		feedback[0+4*i], feedback[1+4*i],
		feedback[2+4*i], feedback[3+4*i]);
    }
#endif

    glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);

    glDeleteProgram(program);
    glDeleteShader(shader);

    glDeleteBuffers(1, &tbo);
    glDeleteBuffers(1, &vbo);

    glDeleteVertexArrays(1, &vao);
}
