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
    print(prefix_counts[:10])

    # Find percentage of flows using top ten prefixes
    total = df.shape[0]
    top_ten_sum = prefix_counts[:10].sum()
    # Print result
    print(top_ten_sum, ' / ', total, ' = ', top_ten_sum/total)

    # Aggregate by bytes
    bytes_per_prefix = df_truncated_ip_addr.groupby('Truncated Src IP addr')['Bytes'].sum().sort_values(ascending=False)
    print(bytes_per_prefix[:10])

    # Find percentage of bytes sent in top 10 flows
    total_bytes = df['Bytes'].sum()
    top_ten_bytes_sum = bytes_per_prefix[:10].sum()
    print(top_ten_bytes_sum, ' / ', total_bytes, ' = ', top_ten_bytes_sum/total_bytes)


    # Begin: Q1.3




if __name__ == '__main__':
    main()