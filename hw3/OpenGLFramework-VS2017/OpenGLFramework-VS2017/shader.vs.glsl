#version 330

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec2 aTexCoord;

out vec2 texCoord;

/* ---------- [HW2] Lighting ---------- */
out vec3 vertex_color;
out vec3 vertex_normal;
out vec3 frag_aPos;

struct LightingAttrib
{
	vec3 position;
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
	vec3 spot_direction;
	float spot_exponent;
	float spot_cutoff;
	float shininess;
	float constant_attenuation;
	float linear_attenuation;
	float quadratic_attenuation;
};

struct PhongMaterial
{
	vec3 Ka;
	vec3 Kd;
	vec3 Ks;
};

// Use "uniform" to represent "constant" values in shaders (cannot be altered during a draw command)
uniform int lighting_mode;
uniform int is_perpixel;
uniform LightingAttrib lighting_attrib[3];   // for directional, point, and spot lights
uniform PhongMaterial material;
/* ------------------------------------ */

uniform mat4 um4p;     // projection
uniform mat4 um4v;     // viewing transformation
uniform mat4 um4m;     // model transformation

// [TODO] passing uniform variable for texture coordinate offset
uniform float x_offset;
uniform float y_offset;
uniform int texture_is_eye;


/* ---------- [HW2] Lighting Functions ---------- */
vec3 directional_light(vec3 v_normal)
{
	vec3 n = normalize(v_normal);
	// Transform the light position from world space to view space
	vec3 view_light_pos = (um4v * vec4(lighting_attrib[0].position, 1.0)).xyz;
	vec3 view_origin_pos = (um4v * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
	// Calculate the normalized light direction vector
	vec3 L = normalize(view_light_pos - view_origin_pos);
	// Calculate the viewpoint direction vector
	vec3 V = -(um4v * um4m * vec4(aPos, 1.0)).xyz;
	// Calculate the unit halfway vector between light direction and viewpoint direction
	vec3 H = normalize(L + V);
	
	vec3 ambient_term = lighting_attrib[0].ambient * material.Ka;
	vec3 diffuse_term = max(dot(L, n), 0) * lighting_attrib[0].diffuse * material.Kd;
	vec3 specular_term = pow(max(dot(H, n), 0), lighting_attrib[0].shininess) * lighting_attrib[0].specular * material.Ks;

	return clamp(ambient_term + diffuse_term + specular_term, 0.0, 1.0);
}

vec3 point_light(vec3 v_normal)
{
	vec3 n = normalize(v_normal);
	// Transform the light position from world space to view space
	vec3 view_light_pos = (um4v * vec4(lighting_attrib[1].position, 1.0)).xyz;
	// Transform the vertex position from world space to view space
	vec3 view_vertex_pos = (um4v * um4m * vec4(aPos, 1.0)).xyz;
	// Calculate the unit vector L that points from the vertex to light position
	vec3 L = normalize(view_light_pos - view_vertex_pos);
	// Calculate the viewpoint direction vector
	vec3 V = -view_vertex_pos;
	// Calculate the unit halfway vector between light direction and viewpoint direction
	vec3 H = normalize(L + V);

	// Calculate the distance between light position and vertex position
	float d = length(view_vertex_pos - view_light_pos);
	// Calculate attenuation factor
	float f_att = 1.0 / (lighting_attrib[1].constant_attenuation + lighting_attrib[1].linear_attenuation * d + lighting_attrib[1].quadratic_attenuation * d * d);
	if (f_att > 1) f_att = 1.0;

	vec3 ambient_term = lighting_attrib[1].ambient * material.Ka;
	vec3 diffuse_term = max(dot(L, n), 0) * lighting_attrib[1].diffuse * material.Kd;
	vec3 specular_term = pow(max(dot(H, n), 0), lighting_attrib[1].shininess) * lighting_attrib[1].specular * material.Ks;

	return clamp(ambient_term + f_att * diffuse_term + f_att * specular_term, 0.0, 1.0);
}

vec3 spot_light(vec3 v_normal)
{
	vec3 n = normalize(v_normal);
	// Transform the light position from world space to view space
	vec3 view_light_pos = (um4v * vec4(lighting_attrib[2].position, 1.0)).xyz;
	// Transform the vertex position from world space to view space
	vec3 view_vertex_pos = (um4v * um4m * vec4(aPos, 1.0)).xyz;
	// Calculate the unit vector L that points from the vertex to light position
	vec3 L = normalize(view_light_pos - view_vertex_pos);
	// Calculate the viewpoint direction vector
	vec3 V = -view_vertex_pos;
	// Calculate the unit halfway vector between light direction and viewpoint direction
	vec3 H = normalize(L + V);

	// Calculate the distance between light position and vertex position
	float d = length(view_vertex_pos - view_light_pos);
	// Calculate attenuation factor
	float f_att = 1.0 / (lighting_attrib[2].constant_attenuation + lighting_attrib[2].linear_attenuation * d + lighting_attrib[2].quadratic_attenuation * d * d);
	if (f_att > 1) f_att = 1.0;

	// Calculate the spotlight effect
	vec3 spot_to_vert = -L;     // unit vector pointing from spotlight to vertex
	vec3 spot_dir = normalize(lighting_attrib[2].spot_direction);     // unit spotlight direction vector
	float v_dot_d = dot(spot_to_vert, spot_dir);

	float spot_effect;
	if (v_dot_d >= cos(radians(lighting_attrib[2].spot_cutoff)))
		spot_effect = pow(max(v_dot_d, 0), lighting_attrib[2].spot_exponent);
	else
		spot_effect = 0;

	vec3 ambient_term = lighting_attrib[2].ambient * material.Ka;
	vec3 diffuse_term = max(dot(L, n), 0) * lighting_attrib[2].diffuse * material.Kd;
	vec3 specular_term = pow(max(dot(H, n), 0), lighting_attrib[2].shininess) * lighting_attrib[2].specular * material.Ks;

	return clamp(ambient_term + spot_effect * f_att * (diffuse_term + specular_term), 0.0, 1.0);
}
/* ---------------------------------------------- */


void main() 
{
	// [TODO]
	if (texture_is_eye == 1)
		texCoord = aTexCoord + vec2(x_offset, y_offset);
	else
		texCoord = aTexCoord;

	gl_Position = um4p * um4v * um4m * vec4(aPos, 1.0);

	// Transform the normal vector from model space to view space
	vec3 v_normal = vec3(transpose(inverse(um4v * um4m)) * vec4(aNormal, 0.0));
	vertex_normal = v_normal;
	
	if (lighting_mode == 0)
		vertex_color = directional_light(v_normal);
	else if (lighting_mode == 1)
		vertex_color = point_light(v_normal);
	else
		vertex_color = spot_light(v_normal);

	// Pass position to fragment shader
	frag_aPos = aPos;
}
