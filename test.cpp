
void test_parallel_del()
{

    std::cout << "test parallel del, " << 8 << " threads"<<std::endl;
    int PER_THREAD = 30;
    for (long i = 0; i < 8 * PER_THREAD; i++)
        insert(pq, i+1, i+1);

    for (long i = 0; i < nthreads; i ++)
        pthread_create (&ts[i], NULL, removemin_thread, (void *)i);

    for (long i = 0; i < nthreads; i ++)
        (void)pthread_join (ts[i], NULL);

    std::cout<<"OK"<<std::endl;
}


void test_parallel_add()
{
    std::cout << "test parallel add, " << 8 << " threads"<<std::endl;
    for (long i = 0; i < 8; i ++) {
        std::cout << "before creatae thread " << i << std::endl;
        pthread_create(&ts[i], NULL, add_thread, (void *) i);
        std::cout << "created thread " << i << std::endl;
    }
    for (long i = 0; i < 8; i ++)
        (void)pthread_join (ts[i], NULL);

    unsigned long new_, old = 0;
    int PER_THREAD = 30;
    for (long i = 0; i < 8 * PER_THREAD; i++) {
        new_ = (long)q.deleteMin();
        assert (old < new_);
        old = new_;
    }
    std::cout<<"OK"<<std::endl;

}

void * add_thread(void *id)
{
    std::cout << "in add_thread" << std::endl;
    long base = 30 * (long)id;
    for(int i = 0; i < 30; i++) {
        std::cout<< "adding " << i << std::endl;
        q.insert(base + i + 1, base + i + 1);
        std::cout << "done adding " << i << std::endl;
    }
    return NULL;


