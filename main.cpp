#include "TestClient.h"

int main() {
    TestClient client;
    std::cout << "=============================\n";
    std::cout << "  OutGameServer 테스트 클라\n";
    std::cout << "=============================\n\n";

    // 로그인 서버 접속
    if (!client.ConnectLoginServer()) {
        std::cerr << "로그인 서버 접속 실패. 종료합니다.\n";
        return 0;
    }

    // 로그인 3번 기회
    bool loginSuccess = false;
    for (int attempt = 1; attempt <= 3; attempt++) {
        std::string id, pw;
        std::cout << "ID: ";
        std::cin >> id;
        std::cout << "PW: ";
        std::cin >> pw;
        std::cout << '\n';

        if (client.TryLogin(id, pw)) {
            loginSuccess = true;
            break;
        }

        if (attempt < 3) {
            std::cout << "다시 시도해주세요. (" << attempt << "/3)\n\n";
        }
    }

    if (!loginSuccess) {
        std::cerr << "로그인 3회 실패. 종료합니다.\n";
        client.Disconnect();
        return 0;
    }

    std::cout << '\n';

    if (!client.ConnectLobby()) {
        std::cerr << "로비 접속 실패. 종료합니다.\n";
        return 0;
    }

    std::cout << '\n';

    while (true) {
        std::cout << "\n================================\n";
        std::cout << "   1.  유저 검색\n";
        std::cout << "   2.  친구 목록\n";
        std::cout << "   3.  친구 요청 관리\n";
        std::cout << "   4.  코스튬 변경\n";
        std::cout << "   5.  파티 따라가기\n";
        std::cout << "   6.  파티 초대\n";
        std::cout << "   7.  파티 초대 관리\n";
        std::cout << "   8.  파티 탈퇴\n";
        std::cout << "   9.  파티 강퇴 (파티장)\n";
        std::cout << "  10.  파티장 위임 (파티장)\n";
        std::cout << "  11.  매칭 시작\n";
        std::cout << "   0.  종료\n";
        std::cout << "================================\n";
        std::cout << "> ";

        uint16_t select;
        std::cin >> select;
        std::cout << '\n';

        switch (select) {
        case 1:  client.SearchUser();          break;
        case 2:  client.ShowFriendList();       break;
        case 3:  client.ManageFriendRequests(); break;
        case 4:  client.ChangeCostume();        break;
        case 5:  client.PartyFollow();          break;
        case 6:  client.PartyInvite();          break;
        case 7:  client.ManagePartyInvites();    break;
        case 8:  client.PartyLeave();           break;
        case 9:  client.PartyKick();            break;
        case 10: client.PartyDelegate();        break;
        case 11: client.MatchStart();           break;
        case 0:
            std::cout << "종료합니다.\n";
            client.Disconnect();
            return 0;
        default:
            std::cout << "잘못된 입력입니다.\n";
            break;
        }
    }

    return 0;
}