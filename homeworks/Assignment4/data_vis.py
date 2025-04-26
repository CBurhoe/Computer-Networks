import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

def main():
    df = pd.read_csv(r'./netflow.csv')
    print(df)
    df_all_flows = df.__deepcopy__()

    # print(df_all_flows)

    # BEGIN: Q1.1
    sns.ecdfplot(data=df_all_flows, x='Bytes', log_scale=True)
    # plt.show() Uncomment for graphs

    df_ip_flows = df_all_flows[df_all_flows['Protocol'] == 'TCP']
    sns.ecdfplot(data=df_ip_flows, x='Bytes', log_scale=True)
    # plt.show() Uncomment for graphs

    df_udp_flows = df_all_flows[df_all_flows['Protocol'] == 'UDP']
    sns.ecdfplot(data=df_udp_flows, x='Bytes', log_scale=True)
    # plt.show() Uncomment for graphs


    # BEGIN: Q1.2
    df_truncated_ip_addr = df.__deepcopy__()
    df_truncated_ip_addr['Truncated Src IP addr'] = df_truncated_ip_addr['Src IP addr'].apply(lambda x: '.'.join(x.split('.', 2)[:2]))
    prefix_counts = df_truncated_ip_addr['Truncated Src IP addr'].value_counts()
    print(prefix_counts[:10], '\n')

    # Find percentage of flows using top ten prefixes
    total = df.shape[0]
    top_ten_sum = prefix_counts[:10].sum()
    # Print result
    print("Percentage of flows using top ten most common prefixes: ", top_ten_sum, ' / ', total, ' = ', top_ten_sum/total, '\n')

    # Aggregate by bytes
    bytes_per_prefix = df_truncated_ip_addr.groupby('Truncated Src IP addr')['Bytes'].sum().sort_values(ascending=False)
    print(bytes_per_prefix[:10], '\n')

    # Find percentage of bytes sent in top 10 flows
    total_bytes = df['Bytes'].sum()
    top_ten_bytes_sum = bytes_per_prefix[:10].sum()
    print("Percentage of bytes sent over the top ten most used prefixes: ", top_ten_bytes_sum, ' / ', total_bytes, ' = ', top_ten_bytes_sum/total_bytes, '\n')


    # Begin: Q1.3
#     Choose port 23: Telnet
    num_telnet_src = df[(df['Src port'] == 23)].shape[0]

    print("Percentage of flows using source port 23: ", num_telnet_src, " / ", total, " = ", num_telnet_src/total, '\n')

    num_telnet_dst = df[(df['Dst port'] == 23)].shape[0]
    print("Percentage of flows using destination port 23: ", num_telnet_dst, " / ", total, " = ", num_telnet_dst/total, '\n')

    # Begin: Q1.4
    num_from_addr = df_truncated_ip_addr[(df_truncated_ip_addr['Truncated Src IP addr'] == '128.112')]['Bytes'].sum()
    print("Percentage of bytes sent by 128.112.0.0/16 block to router: ", num_from_addr, " / ", total_bytes, " = ", num_from_addr/total_bytes)

    df_truncated_ip_addr['Truncated Dst IP addr'] = df_truncated_ip_addr['Dst IP addr'].apply(lambda x: '.'.join(x.split('.', 2)[:2]))

    num_to_addr = df_truncated_ip_addr[(df_truncated_ip_addr['Truncated Dst IP addr'] == '128.112')]['Bytes'].sum()
    print("Percentage of bytes sent by router to 128.112.0.0/16 block: ", num_to_addr, " / ", total_bytes, " = ", num_to_addr/total_bytes, '\n')

    num_within_block = df_truncated_ip_addr[(df_truncated_ip_addr['Truncated Src IP addr'] == '128.112') & (df_truncated_ip_addr['Truncated Dst IP addr'] == '128.112')]['Bytes'].sum()
    print("Percentage of bytes with source and destination address within 128.112.0.0/16 block: ", num_within_block, " / ", total_bytes, " = ", num_within_block/total_bytes, '\n')

    # Q1.5 written

    # Begin Q2.1



if __name__ == '__main__':
    main()