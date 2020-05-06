#include <stdio.h>
#include <string>
#include <thread>
#include <chrono>
#include <list>

#include <string.h>


int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("using: jsontest path file interval count");
        return 0;
    }

    int interval = atoi(argv[3]);
    int count = atoi(argv[4]);

    std::list<std::string> data;

    FILE *fl = fopen(argv[2], "r");
    if (!fl)
    {
        printf("Error at open test file: %s\n", argv[2]);
        return 0;
    }
    char buffer[65536];
    while (!feof(fl))
    {
        if (!fgets(buffer, sizeof(buffer), fl))
            break;
        if(buffer[strlen(buffer)-1]=='\n'){
            buffer[strlen(buffer)-1] = 0;
        }
        data.push_back(std::string(buffer));
    }
    fclose(fl);


    //fprintf("Readed %d lines\n",(int)data.size());

    int counter=0;
    while (true)
    {
        char name[300];
        sprintf(name, "%s/%d.json", argv[1], counter++);

        FILE *fl = fopen(name, "w");
        for (int j = 0; j < count; j++)
        {

            for (auto &el : data)
            {
                fprintf(fl, "%s\n", el.c_str());
            }
        }
        fclose(fl);

        std::this_thread::sleep_for(std::chrono::seconds(interval));
    }

    return 0;
}