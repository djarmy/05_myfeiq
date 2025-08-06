#ifndef FILE_SENDER_H
#define FILE_SENDER_H

#include <sys/socket.h>
#include <netinet/in.h>

/* 
 * ���ܣ�ͨ��UDP�����ļ�֪ͨ������Э����ݣ�
 * ������
 *   ip - Ŀ������IP��ַ�ַ�������"192.168.1.100"��
 *   filepath - �������ļ�������·������"/home/user/doc.txt"��
 * ˵����
 *   1. ����UDP֪ͨ��Ŀ����������֪�ļ���Ϣ
 *   2. �Զ����ļ�ע�ᵽ�ļ�ע���
 *   3. ����get_udp_fd()��ȡUDP�׽���
 */
void sendfile_ipmsg(const char* ip, const char* filepath);

/* 
 * ���ܣ�TCP�ļ������̺߳���������Ϊ�߳����ʹ�ã�
 * ������
 *   arg - �̲߳�����δʹ�ã��ɴ�NULL��
 * ����ֵ��
 *   �߳��˳�״̬��NULL��
 * ˵����
 *   1. ѭ������TCP���ӣ������ļ�����
 *   2. ���������е�packno��fino�����ļ�������
 *   3. ����get_tcp_fd()��ȡTCP�׽���
 */
void *tcp_file_sender_thread(void *arg);

#endif  // FILE_SENDER_H
