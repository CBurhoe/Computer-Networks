import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

def main():
    df = pd.read_csv(r'./netflow.csv')
    print(df)
    df_all_flows = df.__deepcopy__()

    print(df_all_flows)

    # BEGIN: Q1.1
    sns.ecdfplot(data=df_all_flows, x='Bytes', log_scale=True)
    plt.show()

    df_ip_flows = df_all_flows[df_all_flows['Protocol'] == 'TCP']
    sns.ecdfplot(data=df_ip_flows, x='Bytes', log_scale=True)
    plt.show()

    df_udp_flows = df_all_flows[df_all_flows['Protocol'] == 'UDP']
    sns.ecdfplot(data=df_udp_flows, x='Bytes', log_scale=True)
    plt.show()


    # BEGIN: Q1.2
    # df_truncated_ip_addr = df_all_flows[df_all_flows['Protocol'] == 'IP']





if __name__ == '__main__':
    main()