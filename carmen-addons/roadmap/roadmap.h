/*********************************************************
 *
 * This source code is part of the Carnegie Mellon Robot
 * Navigation Toolkit (CARMEN)
 *
 * CARMEN Copyright (c) 2002 Michael Montemerlo, Nicholas
 * Roy, and Sebastian Thrun
 *
 * CARMEN is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public 
 * License as published by the Free Software Foundation; 
 * either version 2 of the License, or (at your option)
 * any later version.
 *
 * CARMEN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied 
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more 
 * details.
 *
 * You should have received a copy of the GNU General 
 * Public License along with CARMEN; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, 
 * Suite 330, Boston, MA  02111-1307 USA
 *
 ********************************************************/

#ifndef ROADMAP_H
#define ROADMAP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int id;
  int x, y;
  int label;
  carmen_list_t *edges;
  double utility;
  int bad;
} carmen_roadmap_vertex_t;

typedef struct {
  int id;
  double cost;
  int blocked;
} carmen_roadmap_edge_t;

typedef struct {
  carmen_list_t *nodes;
  int goal_id;
  carmen_map_p c_space;
  int avoid_people;
} carmen_roadmap_t;

carmen_roadmap_t *carmen_roadmap_initialize(carmen_map_p map);
void carmen_roadmap_plan(carmen_roadmap_t *roadmap, 
			 carmen_world_point_t *goal);
carmen_roadmap_vertex_t *carmen_roadmap_nearest_node
(carmen_world_point_t *point, carmen_roadmap_t *roadmap);
carmen_roadmap_vertex_t *carmen_roadmap_next_node
(carmen_roadmap_vertex_t *node, carmen_roadmap_t *roadmap);
int carmen_roadmap_is_visible(carmen_roadmap_vertex_t *node, 
			      carmen_world_point_t *position,
			      carmen_roadmap_t *roadmap);
carmen_roadmap_vertex_t *carmen_roadmap_best_node
 (carmen_world_point_t *point, carmen_roadmap_t *roadmap);

#ifdef __cplusplus
}
#endif
 
#endif