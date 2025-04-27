import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

def main():
    # question_one_analysis()
    question_two_analysis()


def question_one_analysis():
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

def question_two_one():
    df = pd.read_csv(r'./bgp_route.csv')

    all_ases = df['ASPATH'].str.split().explode()
    as_paths = all_ases.value_counts().reset_index()

    as_paths.columns = ['AS', 'Count']
    print(as_paths.head(10), '\n')
    total_paths = df.shape[0]

    # top_ten = df[df['ASPATH'] as_paths['Count'].head(10).sum()]
    top_ten = df['ASPATH'].apply(lambda x: any(as_name in x.split() for as_name in as_paths.head(10)['AS'])).sum() # Very inefficient, maybe fix later
    print("The top ten most frequently occurring ASes are on ", top_ten, '/', total_paths, " = ", top_ten/total_paths, " paths.", '\n')

def question_two_two():
    df = pd.read_csv(r'./bgp_route.csv')
    df['Path Length'] = df['ASPATH'].str.split().str.len()
    sns.ecdfplot(data=df, x='Path Length')
    plt.show()

def question_two_three():
    df = pd.read_csv(r'./bgp_update.csv')
    df['Closest Second'] = df['TIME'].apply(lambda x: x.split('.', 1)[0])
    df['Closest Minute'] = df['TIME'].apply(lambda x: x.split(':', 1)[0])
    print(df.head(10))

    updates_per_second = df['Closest Second'].value_counts()
    updates_per_minute = df['Closest Minute'].value_counts()
    sns.histplot(data=df, x='Closest Second')
    plt.show()
    # sns.histplot(data=df, x='Closest Minute')
    # plt.show()

    print("Average updates per minute is: ", updates_per_minute.mean())

def question_two_four():
    df1 = pd.read_csv(r'./bgp_update.csv')
    df2 = pd.read_csv(r'./bgp_route.csv')
    total_paths = df2.shape[0] + df1.shape[0]

    from_df1 = df1['FROM'].apply(lambda x: x.split(' ', 1)[1])
    from_df2 = df2['FROM'].apply(lambda x: x.split(' ', 1)[1])

    all_ases = pd.concat([from_df1, from_df2])
    as_counts = all_ases.value_counts().reset_index()
    as_counts.columns = ['AS', 'Count']


    as_counts['Percentage'] = [count / total_paths * 100 for count in as_counts['Count']]
    sns.ecdfplot(data=as_counts, x='Percentage', log_scale=True)
    plt.show()




def question_two_analysis():
    # question_two_one()
    # question_two_two()
    # question_two_three()
    question_two_four()





if __name__ == '__main__':
    main()