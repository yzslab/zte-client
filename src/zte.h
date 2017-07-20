//
// Created by root on 7/19/17.
//

#ifndef ZTE_CLIENT_ZTE_H
#define ZTE_CLIENT_ZTE_H

struct zte;
typedef struct zte zte;
enum status_list;
typedef enum status_list status_list;

zte *createZteClient(const char *, const char *, const char *, int *);
int startZteClient(zte *zteClient);
void stopZteClient(zte *zteClient);


#endif //ZTE_CLIENT_ZTE_H
