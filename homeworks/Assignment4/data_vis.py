import pandas as pd

def main():
    df = pd.read_csv(r'./bgp_route.csv')
    print(df)
    return 0


if __name__ == '__main__':
    main()